#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct CpuTimes
{
    ULONGLONG idle = 0, kernel = 0, user = 0;
};

struct MemInfo
{
    ULONGLONG total = 0, avail = 0, used = 0;
    double percent = 0.0;
};

struct ProcInfo
{
    DWORD pid = 0;
    std::wstring name;
    std::wstring user;
    std::wstring cmdline;
    SIZE_T workingSet = 0;
    DWORD threads = 0;
    double cpu_percent = 0.0;
};

bool GetSystemCpuTimes(CpuTimes &out);
double CalcCpuUsage(const CpuTimes &prev, const CpuTimes &curr);

MemInfo GetMemoryInfo();
