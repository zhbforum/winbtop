#include "metrics.h"
#include <windows.h>

static ULONGLONG FileTimeToULL(const FILETIME &ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

bool GetSystemCpuTimes(CpuTimes &out)
{
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user))
        return false;
    out.idle = FileTimeToULL(idle);
    out.kernel = FileTimeToULL(kernel);
    out.user = FileTimeToULL(user);
    return true;
}

double CalcCpuUsage(const CpuTimes &prev, const CpuTimes &curr)
{
    ULONGLONG idleDelta = curr.idle - prev.idle;
    ULONGLONG kernelDelta = curr.kernel - prev.kernel;
    ULONGLONG userDelta = curr.user - prev.user;
    ULONGLONG busy = (kernelDelta - idleDelta) + userDelta;
    ULONGLONG total = busy + idleDelta;
    if (!total)
        return 0.0;

    long double pct = (long double)busy * 100.0L / (long double)total;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return (double)pct;
}

MemInfo GetMemoryInfo()
{
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    MemInfo m;
    m.total = ms.ullTotalPhys;
    m.avail = ms.ullAvailPhys;
    m.used = m.total - m.avail;
    m.percent = ms.dwMemoryLoad;
    return m;
}
