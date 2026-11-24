// mbChess: portability layer for the Tarrasch Toy Engine (MIT, Bill Forster), which was
// written for Windows only. On Windows the real API is used; elsewhere the few calls the
// engine needs are emulated with the C++ standard library and POSIX.
#ifndef MBCHESS_TARRASCH_PORT_H
#define MBCHESS_TARRASCH_PORT_H

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <mutex>
#include <strings.h>
#include <thread>

typedef int errno_t;
typedef void *HANDLE;
typedef void *(*LPTHREAD_START_ROUTINE)(void *);
#define INFINITE 0xFFFFFFFFu

inline unsigned long GetTickCount()
{
    using namespace std::chrono;
    return (unsigned long)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

inline int _strcmpi(const char *a, const char *b) { return strcasecmp(a, b); }

inline errno_t strcpy_s(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0)
        return EINVAL;
    std::strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
    return 0;
}

inline errno_t fopen_s(FILE **f, const char *name, const char *mode)
{
    *f = std::fopen(name, mode);
    return *f ? 0 : errno;
}

inline errno_t localtime_s(struct tm *out, const time_t *t)
{
    return localtime_r(t, out) ? 0 : EINVAL;
}

inline errno_t asctime_s(char *buf, size_t size, const struct tm *tm)
{
    char tmp[64];
    if (!asctime_r(tm, tmp))
        return EINVAL;
    return strcpy_s(buf, size, tmp);
}

// A counting semaphore and a thread, as far as toyengine.cpp uses them.
struct MbcSemaphore
{
    std::mutex mutex;
    std::condition_variable cond;
    long count = 0;
};

inline HANDLE CreateSemaphore(void *, long initial, long, void *)
{
    auto *s = new MbcSemaphore;
    s->count = initial;
    return s;
}

inline int ReleaseSemaphore(HANDLE h, long n, long *)
{
    auto *s = static_cast<MbcSemaphore *>(h);
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        s->count += n;
    }
    s->cond.notify_all();
    return 1;
}

inline unsigned long WaitForSingleObject(HANDLE h, unsigned long)
{
    auto *s = static_cast<MbcSemaphore *>(h);
    std::unique_lock<std::mutex> lock(s->mutex);
    s->cond.wait(lock, [s] { return s->count > 0; });
    --s->count;
    return 0;
}

inline HANDLE CreateThread(void *, size_t, LPTHREAD_START_ROUTINE start, void *arg, unsigned long, unsigned long *id)
{
    std::thread t([start, arg] { start(arg); });
    t.detach(); // the reader thread ends with the process
    if (id)
        *id = 1;
    return nullptr;
}

inline int CloseHandle(HANDLE) { return 1; } // the semaphore lives until the process ends

#define _get_osfhandle(fd) ((void)(fd), (HANDLE) nullptr)
#define _fileno fileno
#endif

#endif
