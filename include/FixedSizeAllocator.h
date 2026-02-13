#ifndef FIXEDSIZEALLOCATOR_H
#define FIXEDSIZEALLOCATOR_H

#include <stddef.h>
#include <string.h>
#include <new>

#include "XArray.h"
#include "XBitArray.h"

class XFixedSizeAllocator
{
public:
    enum { DEFAULT_CHUNK_SIZE = 4096 };

    XFixedSizeAllocator(size_t iBlockSize, size_t iPageSize = DEFAULT_CHUNK_SIZE)
        : m_PageSize(iPageSize),
          m_BlockSize(iBlockSize < sizeof(size_t) ? sizeof(size_t) : iBlockSize), // Ensure block size can store free-list link.
          m_AChunk(NULL), m_DChunk(NULL)
    {
        // Align block size to allow safe reads/writes of the free-list link (stored as size_t).
        const size_t align = sizeof(size_t);
        m_BlockSize = (m_BlockSize + (align - 1)) & ~(align - 1);

        // Calculate how many blocks fit in a page
        m_BlockCount = m_PageSize / m_BlockSize;
        if (m_BlockCount == 0)
            m_BlockCount = 1; // At least one block per chunk
    }

    ~XFixedSizeAllocator()
    {
        Clear();
    }

    // return the number of allocated chunks
    size_t GetChunksCount()
    {
        return m_Chunks.Size();
    }

    // return the total size of all chunks
    size_t GetChunksTotalSize()
    {
        return m_Chunks.Size() * m_PageSize;
    }

    // return the occupied memory in chunks
    size_t GetChunksOccupation()
    {
        size_t occupation = 0;
        for (Chunks::Iterator it = m_Chunks.Begin(); it != m_Chunks.End(); ++it)
        {
            Chunk *c = *it;
            if (!c)
                continue;
            occupation += (c->m_BlockCount - c->m_BlockAvailables) * m_BlockSize;
        }
        return occupation;
    }

    template <class T>
    void CallDtor(T *iDummy)
    {
        // we clear all the chunks
        for (Chunks::Iterator it = m_Chunks.Begin(); it != m_Chunks.End(); ++it)
        {
            Chunk *c = *it;
            if (!c)
                continue;
            c->CallDtor(iDummy, m_BlockSize, m_BlockCount);
        }
    }

    void Clear()
    {
        // Destroy all chunks
        for (Chunks::Iterator it = m_Chunks.Begin(); it != m_Chunks.End(); ++it)
        {
            Chunk *c = *it;
            if (!c)
                continue;
            delete c;
        }
        m_Chunks.Clear();
        m_AChunk = NULL;
        m_DChunk = NULL;
    }

    void *Allocate()
    {
        // Try to allocate from current allocating chunk
        if (m_AChunk && m_AChunk->m_BlockAvailables > 0)
        {
            return m_AChunk->Allocate(m_BlockSize);
        }

        // Find a chunk with available blocks
        for (Chunks::Iterator it = m_Chunks.Begin(); it != m_Chunks.End(); ++it)
        {
            Chunk *c = *it;
            if (c && c->m_BlockAvailables > 0)
            {
                m_AChunk = c;
                return m_AChunk->Allocate(m_BlockSize);
            }
        }

        // No chunks with available blocks, create a new one
        Chunk *newChunk = new Chunk();
        newChunk->Init(m_BlockSize, m_BlockCount);
        m_Chunks.PushBack(newChunk);
        m_AChunk = newChunk;
        return m_AChunk->Allocate(m_BlockSize);
    }

    void Free(void *iP)
    {
        if (!iP)
            return;

        // Find the chunk containing this pointer
        Chunk *chunk = FindChunk(iP);
        if (chunk)
        {
            chunk->Deallocate(iP, m_BlockSize);
            m_DChunk = chunk; // Cache for next deallocation
        }
    }

private:
    class Chunk
    {
    public:
        Chunk() : m_Data(NULL), m_FirstAvailableBlock(INVALID_BLOCK_INDEX), m_BlockAvailables(0), m_BlockCount(0) {}

        ~Chunk()
        {
            Destroy();
        }

        void Init(size_t iBlockSize, size_t iBlockCount)
        {
            m_BlockCount = iBlockCount;
            m_BlockAvailables = iBlockCount;
            m_FirstAvailableBlock = 0;

            // Allocate memory for the chunk
            m_Data = new unsigned char[iBlockCount * iBlockSize];

            // Initialize free list - each free block contains index of next free block.
            for (size_t i = 0; i + 1 < iBlockCount; ++i)
            {
                WriteFreeListIndex(m_Data + i * iBlockSize, i + 1);
            }
            // Last block marks end of free list.
            WriteFreeListIndex(m_Data + (iBlockCount - 1) * iBlockSize, INVALID_BLOCK_INDEX);
        }

        template <class T>
        void CallDtor(T *iDummy, size_t iBlockSize, size_t iBlockCount)
        {
            // everything is clear -> nothing to do
            if (m_BlockAvailables == iBlockCount)
                return;

            // else we have some cleaning todo

            if (!m_BlockAvailables)
            {
                // we need to clean everything
                // we call the dtor of the used blocks
                for (size_t i = 0; i < iBlockCount; ++i)
                {

                    unsigned char *p = m_Data + i * iBlockSize;
                    ((T *)p)->~T();
                }
            }
            else
            {
                // only some are used
                XBitArray freeBlocks;

                {
                    // we mark the objects used
                    size_t freeb = m_FirstAvailableBlock;
                    size_t marked = 0;
                    while (marked < m_BlockAvailables && freeb != INVALID_BLOCK_INDEX)
                    {
                        freeBlocks.Set(freeb);
                        unsigned char *p = m_Data + freeb * iBlockSize;
                        freeb = ReadFreeListIndex(p);
                        ++marked;
                    }
                }

                {
                    // we call the dtor of the used blocks
                    for (size_t i = 0; i < iBlockCount; ++i)
                    {

                        if (freeBlocks.IsSet(i))
                            continue;

                        unsigned char *p = m_Data + i * iBlockSize;
                        ((T *)p)->~T();
                    }
                }
            }
        }

        void Destroy()
        {
            delete[] m_Data;
            m_Data = NULL;
            m_BlockCount = 0;
            m_BlockAvailables = 0;
            m_FirstAvailableBlock = INVALID_BLOCK_INDEX;
        }

        void *Allocate(size_t iBlockSize)
        {
            if (m_BlockAvailables == 0)
            {
                return NULL; // No available blocks
            }

            // Get the first available block
            unsigned char *result = m_Data + m_FirstAvailableBlock * iBlockSize;

            // Update the free list head to next available block
            m_FirstAvailableBlock = ReadFreeListIndex(result);
            m_BlockAvailables--;

            return result;
        }

        void Deallocate(void *iP, size_t iBlockSize)
        {
            if (!iP)
                return;

            // Calculate block index within this chunk.
            unsigned char *ptr = static_cast<unsigned char *>(iP);
            ptrdiff_t offset = ptr - m_Data;
            if (offset < 0)
            {
                return; // Invalid pointer.
            }

            size_t byteOffset = static_cast<size_t>(offset);
            // Validate the pointer is properly aligned.
            if ((byteOffset % iBlockSize) != 0)
            {
                return; // Invalid pointer.
            }
            size_t blockIndex = byteOffset / iBlockSize;

            if (blockIndex >= m_BlockCount)
            {
                return; // Out of range.
            }

            if (m_BlockAvailables >= m_BlockCount)
            {
                return; // Likely double free or corruption.
            }

            // Add this block to the front of the free list.
            WriteFreeListIndex(static_cast<unsigned char *>(iP), m_FirstAvailableBlock);
            m_FirstAvailableBlock = blockIndex;
            m_BlockAvailables++;
        }

        static void WriteFreeListIndex(unsigned char *iBlock, size_t iIndex)
        {
            memcpy(iBlock, &iIndex, sizeof(size_t));
        }

        static size_t ReadFreeListIndex(const unsigned char *iBlock)
        {
            size_t index = INVALID_BLOCK_INDEX;
            memcpy(&index, iBlock, sizeof(size_t));
            return index;
        }

        unsigned char *m_Data;
        size_t m_FirstAvailableBlock;
        size_t m_BlockAvailables;
        size_t m_BlockCount;

        static const size_t INVALID_BLOCK_INDEX = static_cast<size_t>(-1);
    };

    // types
    typedef XArray<Chunk *> Chunks;

    // function to find the chunk containing the ptr
    Chunk *FindChunk(void *iP)
    {
        // Check cached deallocating chunk first for performance
        if (m_DChunk)
        {
            unsigned char *ptr = (unsigned char *)iP;
            unsigned char *chunkStart = m_DChunk->m_Data;
            unsigned char *chunkEnd = chunkStart + (m_DChunk->m_BlockCount * m_BlockSize);
            if (ptr >= chunkStart && ptr < chunkEnd)
            {
                return m_DChunk;
            }
        }

        // Search all chunks
        for (Chunks::Iterator it = m_Chunks.Begin(); it != m_Chunks.End(); ++it)
        {
            Chunk *c = *it;
            if (!c)
                continue;
            unsigned char *ptr = (unsigned char *)iP;
            unsigned char *chunkStart = c->m_Data;
            unsigned char *chunkEnd = chunkStart + (c->m_BlockCount * m_BlockSize);
            if (ptr >= chunkStart && ptr < chunkEnd)
            {
                return c;
            }
        }

        return NULL; // Not found
    }

    // members
    size_t m_PageSize;
    // Block size
    size_t m_BlockSize;
    // Blocks Count (per Chunk)
    size_t m_BlockCount;

    // the chunks
    Chunks m_Chunks;

    // Allocating and deallocating chunks
    Chunk *m_AChunk;
    Chunk *m_DChunk;
};

//
template <class T>
class XObjectPool
{
public:
    XObjectPool(XBOOL iCallDtor = TRUE) : m_Allocator(sizeof(T)), m_CallDtor(iCallDtor) {}

    T *Allocate()
    {
        void *mem = m_Allocator.Allocate();
        if (!mem)
            return NULL;
        return new (mem) T;
    }

    void Free(T *iP)
    {
        if (!iP)
            return;
        if (m_CallDtor)
            iP->~T();

        m_Allocator.Free(iP);
    }

    void Clear()
    {
        if (m_CallDtor)
            m_Allocator.CallDtor((T *)NULL);

        m_Allocator.Clear();
    }

private:
    XFixedSizeAllocator m_Allocator;
    XBOOL m_CallDtor;
};

#endif // FIXEDSIZEALLOCATOR_H
