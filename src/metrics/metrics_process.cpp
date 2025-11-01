#include "metrics_process.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <memory>
#include <sddl.h>
#include <winternl.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

static inline ULONGLONG FileTimeToULL(const FILETIME &ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

bool SnapshotProcesses(std::vector<ProcRaw> &out)
{
    out.clear();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (!Process32FirstW(snap, &pe))
    {
        CloseHandle(snap);
        return false;
    }

    do
    {
        ProcRaw pr{};
        pr.pid = pe.th32ProcessID;
        pr.ppid = pe.th32ParentProcessID;
        pr.threads = pe.cntThreads;
        pr.kernel = 0;
        pr.user = 0;
        pr.workingSet = 0;

        pr.imageName = pe.szExeFile;

        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pr.pid);
        if (h)
        {
            FILETIME ct{}, et{}, kt{}, ut{};
            if (GetProcessTimes(h, &ct, &et, &kt, &ut))
            {
                pr.kernel = FileTimeToULL(kt);
                pr.user = FileTimeToULL(ut);
            }
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
            {
                pr.workingSet = pmc.WorkingSetSize;
            }
            CloseHandle(h);
        }

        out.push_back(std::move(pr));
    } while (Process32NextW(snap, &pe));

    CloseHandle(snap);
    return true;
}

static std::wstring GetProcessBaseNameFromHandle(HANDLE h)
{
    wchar_t buf[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(h, 0, buf, &size))
    {
        std::wstring full(buf, size);
        size_t pos = full.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? full : full.substr(pos + 1);
    }
    return L"?";
}

std::wstring GetProcessBaseNameLazy(DWORD pid)
{
    std::wstring name = L"?";
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h)
        return name;
    name = GetProcessBaseNameFromHandle(h);
    CloseHandle(h);
    return name;
}

typedef NTSTATUS(NTAPI *PFN_NtQueryInformationProcess)(
    HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

std::wstring GetProcessCommandLineLazy(DWORD pid)
{
    std::wstring out;

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return out;

    auto NtQueryInformationProcess =
        reinterpret_cast<PFN_NtQueryInformationProcess>(
            GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (!NtQueryInformationProcess)
        return out;

    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h)
        return out;

    PROCESS_BASIC_INFORMATION pbi{};
    if (NtQueryInformationProcess(h, ProcessBasicInformation, &pbi, sizeof(pbi), nullptr) < 0)
    {
        CloseHandle(h);
        return out;
    }

    PEB peb{};
    SIZE_T rd = 0;
    if (!ReadProcessMemory(h, pbi.PebBaseAddress, &peb, sizeof(peb), &rd) || rd != sizeof(peb))
    {
        CloseHandle(h);
        return out;
    }

    RTL_USER_PROCESS_PARAMETERS params{};
    if (!ReadProcessMemory(h, peb.ProcessParameters, &params, sizeof(params), &rd) || rd != sizeof(params))
    {
        CloseHandle(h);
        return out;
    }

    if (params.CommandLine.Buffer && params.CommandLine.Length)
    {
        std::wstring buf(params.CommandLine.Length / sizeof(WCHAR), L'\0');
        SIZE_T need = params.CommandLine.Length, got = 0;
        if (ReadProcessMemory(h, params.CommandLine.Buffer, buf.data(), need, &got) && got == need)
        {
            out = std::move(buf);
        }
    }

    CloseHandle(h);
    return out;
}

std::wstring GetProcessUserLazy(DWORD pid)
{
    std::wstring result;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return result;

    HANDLE token = nullptr;
    if (OpenProcessToken(h, TOKEN_QUERY, &token))
    {
        DWORD len = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &len);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            std::unique_ptr<BYTE[]> buf(new BYTE[len]);
            if (GetTokenInformation(token, TokenUser, buf.get(), len, &len))
            {
                auto tu = reinterpret_cast<TOKEN_USER *>(buf.get());
                wchar_t name[256], domain[256];
                DWORD cchName = 256, cchDomain = 256;
                SID_NAME_USE use;
                if (LookupAccountSidW(nullptr, tu->User.Sid, name, &cchName, domain, &cchDomain, &use))
                {
                    result = std::wstring(domain) + L"\\" + name;
                }
            }
        }
        CloseHandle(token);
    }
    CloseHandle(h);
    return result;
}
