/**
 * @file VxThreadSDL3.cpp
 * @brief SDL3 implementation of VxThread threading API.
 *
 * This implementation uses SDL3's threading API for portable thread
 * management across all platforms.
 */

#include "VxThread.h"

#include <SDL3/SDL_thread.h>
#include <string.h>

// Static member for main thread tracking
VxThread *VxThread::m_MainThread = NULL;

// Forward declaration of thread wrapper
int SDLCALL VxThreadSDL3Wrapper(void *data);

VxThread::VxThread() : m_Name(), m_Thread(NULL), m_ThreadID(0), m_Priority(0), m_Func(NULL), m_Args(NULL) {
    m_State = VXTS_JOINABLE;
}

VxThread::~VxThread() {
    GetMutex().EnterMutex();
    GetHashThread().Remove(m_Thread);
    GetMutex().LeaveMutex();
}

// Internal thread entry point for SDL3
unsigned long VX_STDCALL VxThread::ThreadFunc(void *args) {
    if (!args)
        return VXTERROR_NULLTHREAD;

    VxThread *thread = (VxThread *)args;
    thread->m_State |= VXTS_STARTED;

    XDWORD ret;
    if (thread->m_Func)
        ret = thread->m_Func(thread->m_Args);
    else
        ret = thread->Run();

    thread->m_State = VXTS_INITIALE;
    return ret;
}

// SDL3 thread wrapper function - calls the internal ThreadFunc
int SDLCALL VxThreadSDL3Wrapper(void *data) {
    if (!data)
        return VXTERROR_NULLTHREAD;

    VxThread *thread = (VxThread *)data;
    thread->m_State |= VXTS_STARTED;

    int ret;
    if (thread->m_Func)
        ret = thread->m_Func(thread->m_Args);
    else
        ret = thread->Run();

    thread->m_State = VXTS_INITIALE;
    return ret;
}

XBOOL VxThread::CreateThread(VxThreadFunction *func, void *args) {
    if (IsCreated())
        return TRUE;

    m_Func = func;
    m_Args = args;

    // Create SDL thread with a name
    const char *threadName = m_Name.Length() > 0 ? m_Name.CStr() : "VxThread";
    SDL_Thread *sdlThread = SDL_CreateThread(VxThreadSDL3Wrapper, threadName, this);
    if (!sdlThread)
        return FALSE;

    m_Thread = (GENERIC_HANDLE)sdlThread;
    m_ThreadID = (XUINTPTR)SDL_GetThreadID(sdlThread);
    m_State |= VXTS_CREATED;

    if (m_Name.Length() == 0) {
        m_Name = "THREAD_";
        m_Name << (int)(intptr_t)m_Thread;
    }

    GetMutex().EnterMutex();
    GetHashThread().Insert(m_Thread, this);
    GetMutex().LeaveMutex();

    SetPriority();

    return TRUE;
}

void VxThread::SetPriority(unsigned int priority) {
    m_Priority = priority;
    if (IsCreated())
        SetPriority();
}

void VxThread::SetName(const char *name) {
    m_Name = name;
}

void VxThread::Close() {
    // SDL threads are cleaned up by SDL_WaitThread or SDL_DetachThread
    if (m_Thread) {
        SDL_DetachThread((SDL_Thread *)m_Thread);
    }
    m_ThreadID = 0;
    m_Thread = NULL;
    m_State = 0;
    m_Priority = 0;
    m_Func = NULL;
    m_Args = NULL;
}

const XString &VxThread::GetName() const {
    return m_Name;
}

unsigned int VxThread::GetPriority() const {
    return m_Priority;
}

XBOOL VxThread::IsCreated() const {
    return (m_State & VXTS_CREATED) != 0;
}

XBOOL VxThread::IsJoinable() const {
    return (m_State & VXTS_JOINABLE) != 0;
}

XBOOL VxThread::IsMainThread() const {
    return (m_State & VXTS_MAIN) != 0;
}

XBOOL VxThread::IsStarted() const {
    return (m_State & VXTS_STARTED) != 0;
}

VxThread *VxThread::GetCurrentVxThread() {
    SDL_ThreadID currentId = SDL_GetCurrentThreadID();

    GetMutex().EnterMutex();
    XHashTable<VxThread *, GENERIC_HANDLE>::Iterator it = GetHashThread().Begin();
    for (; it != GetHashThread().End(); ++it) {
        VxThread *thread = *it;
        if (thread && thread->m_ThreadID == currentId) {
            GetMutex().LeaveMutex();
            return thread;
        }
    }
    GetMutex().LeaveMutex();
    return NULL;
}

int VxThread::Wait(unsigned int *status, unsigned int timeout) {
    if (!m_Thread)
        return VXTERROR_NULLTHREAD;

    // SDL3 doesn't have a timed wait, so we use SDL_WaitThread which blocks indefinitely
    // For timeout support, we would need platform-specific code or a polling approach
    (void)timeout; // SDL3 doesn't support timed wait

    int retval = 0;
    SDL_WaitThread((SDL_Thread *)m_Thread, &retval);

    if (status)
        *status = (unsigned int)retval;

    m_Thread = NULL; // Thread handle is invalid after SDL_WaitThread
    return VXT_OK;
}

const GENERIC_HANDLE VxThread::GetHandle() const {
    return m_Thread;
}

XUINTPTR VxThread::GetID() const {
    return m_ThreadID;
}

XBOOL VxThread::GetExitCode(unsigned int &status) {
    // SDL3 doesn't provide a way to get exit code without waiting
    if (m_State & VXTS_STARTED) {
        status = VXT_STILLACTIVE;
        return TRUE;
    }
    return FALSE;
}

XBOOL VxThread::Terminate(unsigned int *status) {
    // SDL3 doesn't support thread termination directly
    // This is intentional as forcible termination is unsafe
    if (status)
        *status = VXT_TERMINATEFORCED;
    return FALSE;
}

XUINTPTR VxThread::GetCurrentVxThreadId() {
    return (XUINTPTR)SDL_GetCurrentThreadID();
}

void VxThread::SetPriority() {
    // SDL3 provides SDL_SetThreadPriority but it affects the current thread only
    // For now, we store the priority but don't apply it (SDL limitation)
    // TODO: Apply priority when thread starts using SDL_SetThreadPriority
}

VxMutex &VxThread::GetMutex() {
    static VxMutex threadMutex;
    return threadMutex;
}

XHashTable<VxThread *, GENERIC_HANDLE> &VxThread::GetHashThread() {
    static XHashTable<VxThread *, GENERIC_HANDLE> hashThread;
    return hashThread;
}
