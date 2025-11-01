#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct ProcRaw
{
    DWORD pid = 0;
    DWORD ppid = 0;
    ULONGLONG kernel = 0;
    ULONGLONG user = 0;
    SIZE_T workingSet = 0;
    DWORD threads = 0;
    std::wstring imageName;
};

bool SnapshotProcesses(std::vector<ProcRaw> &out);

std::wstring GetProcessCommandLineLazy(DWORD pid);
std::wstring GetProcessUserLazy(DWORD pid);
std::wstring GetProcessBaseNameLazy(DWORD pid);
