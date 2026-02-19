#include "VxThread.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

static XUINTPTR VxThreadIdFromNative(pthread_t threadId) {
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&threadId);
    XUINTPTR folded = 0;
    for (size_t index = 0; index < sizeof(pthread_t); ++index) {
        folded = (folded * static_cast<XUINTPTR>(131)) + static_cast<XUINTPTR>(bytes[index]);
    }
    return folded;
}

static thread_local VxThread *g_CurrentVxThread = NULL;

VxThread *VxThread::m_MainThread = NULL;

VxThread::VxThread()
    : m_Name(),
      m_Thread(NULL),
      m_ThreadID(0),
      m_State(VXTS_JOINABLE),
      m_Priority(VXTP_NORMAL),
      m_Func(NULL),
      m_Args(NULL) {
    if (!m_MainThread) {
        m_MainThread = this;
        m_State |= (VXTS_MAIN | VXTS_CREATED | VXTS_STARTED);
        m_ThreadID = GetCurrentVxThreadId();
        g_CurrentVxThread = this;
    }
}

VxThread::~VxThread() {
    if (m_Thread) {
        GetHashThread().Remove(m_Thread);
    }
}

XBOOL VxThread::CreateThread(VxThreadFunction *func, void *args) {
    if (IsCreated()) {
        return TRUE;
    }

    pthread_t *threadHandle = new pthread_t();
    m_Func = func;
    m_Args = args;

    if (pthread_create(threadHandle, NULL, &VxThread::ThreadEntryPoint, this) != 0) {
        delete threadHandle;
        return FALSE;
    }

    m_Thread = static_cast<GENERIC_HANDLE>(threadHandle);
    m_ThreadID = VxThreadIdFromNative(*threadHandle);
    m_State |= VXTS_CREATED;

    if (m_Name.Length() == 0) {
        m_Name = "THREAD_";
        m_Name << static_cast<unsigned long long>(m_ThreadID);
    }

    VxMutexLock lock(GetMutex());
    GetHashThread().Insert(m_Thread, this);

    SetPriority();
    return TRUE;
}

void VxThread::SetPriority(unsigned int priority) {
    m_Priority = priority;
    if (IsCreated()) {
        SetPriority();
    }
}

void VxThread::SetName(const char *name) {
    m_Name = name ? name : "";
}

void VxThread::Close() {
    if (m_Thread) {
        VxMutexLock lock(GetMutex());
        GetHashThread().Remove(m_Thread);
        delete static_cast<pthread_t *>(m_Thread);
    }

    m_Thread = NULL;
    m_ThreadID = 0;
    m_State = 0;
    m_Priority = VXTP_NORMAL;
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
    if (g_CurrentVxThread) {
        return g_CurrentVxThread;
    }
    return m_MainThread;
}

int VxThread::Wait(unsigned int *status, unsigned int timeout) {
    if (!m_Thread) {
        return VXTERROR_NULLTHREAD;
    }

    unsigned int localStatus = 0;
    if (!status) {
        status = &localStatus;
    }

    pthread_t *threadHandle = static_cast<pthread_t *>(m_Thread);
    void *retval = NULL;

    if (timeout == 0) {
        if (pthread_join(*threadHandle, &retval) != 0) {
            return VXTERROR_WAIT;
        }
    } else {
#if defined(__linux__)
        const unsigned int pollMs = 1;
        unsigned int waited = 0;
        int joinRc = 0;
        do {
            joinRc = pthread_tryjoin_np(*threadHandle, &retval);
            if (joinRc == 0) {
                break;
            }
            if (joinRc != EBUSY) {
                return VXTERROR_WAIT;
            }
            usleep(pollMs * 1000);
            waited += pollMs;
        } while (waited < timeout);

        if (joinRc != 0) {
            return VXTERROR_TIMEOUT;
        }
#else
        (void) timeout;
        if (pthread_join(*threadHandle, &retval) != 0) {
            return VXTERROR_WAIT;
        }
#endif
    }

    // Thread exit codes are defined as 32-bit values (see VxThreadFunction typedef).
    // The upper 32 bits of the void* return value are intentionally discarded.
    *status = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(retval) & 0xFFFFFFFFu);
    m_State = VXTS_INITIALE;
    return VXT_OK;
}

const GENERIC_HANDLE VxThread::GetHandle() const {
    return m_Thread;
}

XUINTPTR VxThread::GetID() const {
    return m_ThreadID;
}

XBOOL VxThread::GetExitCode(unsigned int &status) {
    if (!m_Thread) {
        return FALSE;
    }

#if defined(__linux__)
    pthread_t *threadHandle = static_cast<pthread_t *>(m_Thread);
    if (pthread_kill(*threadHandle, 0) == 0) {
        status = VXT_STILLACTIVE;
    } else {
        status = VXT_OK;
    }
#else
    status = VXT_STILLACTIVE;
#endif
    return TRUE;
}

XBOOL VxThread::Terminate(unsigned int *status) {
    if (!m_Thread) {
        return FALSE;
    }

    pthread_t *threadHandle = static_cast<pthread_t *>(m_Thread);
    if (pthread_cancel(*threadHandle) != 0) {
        return FALSE;
    }

    if (status) {
        *status = VXT_TERMINATEFORCED;
    }
    return TRUE;
}

XUINTPTR VxThread::GetCurrentVxThreadId() {
    return VxThreadIdFromNative(pthread_self());
}

void VxThread::SetPriority() {
    if (!m_Thread) {
        return;
    }

    pthread_t *threadHandle = static_cast<pthread_t *>(m_Thread);

    int policy = SCHED_OTHER;
    sched_param param;
    param.sched_priority = 0;

    switch (m_Priority) {
    case VXTP_TIMECRITICAL:
    case VXTP_HIGHLEVEL:
        policy = SCHED_RR;
        param.sched_priority = sched_get_priority_max(SCHED_RR);
        break;
    case VXTP_ABOVENORMAL:
        policy = SCHED_RR;
        param.sched_priority = (sched_get_priority_max(SCHED_RR) + sched_get_priority_min(SCHED_RR)) / 2;
        break;
    case VXTP_BELOWNORMAL:
    case VXTP_LOWLEVEL:
    case VXTP_IDLE:
    case VXTP_NORMAL:
    default:
        policy = SCHED_OTHER;
        param.sched_priority = 0;
        break;
    }

    pthread_setschedparam(*threadHandle, policy, &param);
}

VxMutex &VxThread::GetMutex() {
    static VxMutex threadMutex;
    return threadMutex;
}

XHashTable<VxThread *, GENERIC_HANDLE> &VxThread::GetHashThread() {
    static XHashTable<VxThread *, GENERIC_HANDLE> hashThread;
    return hashThread;
}

unsigned long VX_STDCALL VxThread::ThreadFunc(void *args) {
    if (!args) {
        return VXTERROR_NULLTHREAD;
    }

    VxThread *thread = static_cast<VxThread *>(args);
    thread->m_State |= VXTS_STARTED;

    unsigned long ret;
    if (thread->m_Func) {
        ret = thread->m_Func(thread->m_Args);
    } else {
        ret = thread->Run();
    }

    thread->m_State = VXTS_INITIALE;
    return ret;
}

void *VxThread::ThreadEntryPoint(void *args) {
    VxThread *thread = static_cast<VxThread *>(args);
    if (!thread) {
        return reinterpret_cast<void *>(static_cast<uintptr_t>(VXTERROR_NULLTHREAD));
    }

    g_CurrentVxThread = thread;
    unsigned long ret = ThreadFunc(args);
    return reinterpret_cast<void *>(static_cast<uintptr_t>(ret));
}
