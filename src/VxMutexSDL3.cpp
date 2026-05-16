/**
 * @file VxMutexSDL3.cpp
 * @brief SDL3 implementation of VxMutex synchronization primitive.
 *
 * This implementation uses SDL3's mutex API for portable thread
 * synchronization across all platforms.
 */

#include "VxMutex.h"

#include <SDL3/SDL_mutex.h>

VxMutex::VxMutex() {
    m_Mutex = (void *)SDL_CreateMutex();
}

VxMutex::~VxMutex() {
    if (m_Mutex) {
        SDL_DestroyMutex((SDL_Mutex *)m_Mutex);
    }
}

XBOOL VxMutex::EnterMutex() {
    if (!m_Mutex)
        return FALSE;
    SDL_LockMutex((SDL_Mutex *)m_Mutex);
    return TRUE;
}

XBOOL VxMutex::LeaveMutex() {
    if (!m_Mutex)
        return FALSE;
    SDL_UnlockMutex((SDL_Mutex *)m_Mutex);
    return TRUE;
}

XBOOL VxMutex::operator++(int) {
    return EnterMutex();
}

XBOOL VxMutex::operator--(int) {
    return LeaveMutex();
}
