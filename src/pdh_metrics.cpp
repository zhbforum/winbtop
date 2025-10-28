#include "pdh_metrics.h"

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <cwchar>

#include "util.h"

static PDH_HQUERY qCpu = nullptr;
static PDH_HQUERY qDisk = nullptr;
static PDH_HQUERY qNet = nullptr;

static PDH_HCOUNTER ctrCpuAll = nullptr;
static PDH_HCOUNTER ctrDiskRead = nullptr;
static PDH_HCOUNTER ctrDiskWrite = nullptr;
static std::vector<PDH_HCOUNTER> netBytesTotal;

bool PdhInit()
{
    if (PdhOpenQuery(nullptr, 0, &qCpu) != ERROR_SUCCESS)
        return false;
    if (PdhAddEnglishCounter(qCpu, L"\\Processor(*)\\% Processor Time", 0, &ctrCpuAll) != ERROR_SUCCESS)
        return false;
    PdhCollectQueryData(qCpu);

    if (PdhOpenQuery(nullptr, 0, &qDisk) == ERROR_SUCCESS)
    {
        PdhAddEnglishCounter(qDisk, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &ctrDiskRead);
        PdhAddEnglishCounter(qDisk, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &ctrDiskWrite);
        PdhCollectQueryData(qDisk);
    }
    if (PdhOpenQuery(nullptr, 0, &qNet) == ERROR_SUCCESS)
    {
        PDH_HCOUNTER recvAll = nullptr;
        PDH_HCOUNTER sentAll = nullptr;
        PdhAddEnglishCounter(qNet, L"\\Network Interface(*)\\Bytes Received/sec", 0, &recvAll);
        PdhAddEnglishCounter(qNet, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &sentAll);
        netBytesTotal.clear();
        if (recvAll)
            netBytesTotal.push_back(recvAll);
        if (sentAll)
            netBytesTotal.push_back(sentAll);
        PdhCollectQueryData(qNet);
    }

    return true;
}

void PdhShutdown()
{
    if (qCpu)
    {
        PdhCloseQuery(qCpu);
        qCpu = nullptr;
        ctrCpuAll = nullptr;
    }
    if (qDisk)
    {
        PdhCloseQuery(qDisk);
        qDisk = nullptr;
        ctrDiskRead = nullptr;
        ctrDiskWrite = nullptr;
    }
    if (qNet)
    {
        PdhCloseQuery(qNet);
        qNet = nullptr;
        netBytesTotal.clear();
    }
}

static std::wstring humanRate(double bytesPerSec)
{
    if (!std::isfinite(bytesPerSec) || bytesPerSec < 0)
        bytesPerSec = 0.0;
    return FormatBytesULONGLONG((ULONGLONG)bytesPerSec) + L"/s";
}

std::vector<double> PdhSamplePerCoreCpu()
{
    std::vector<double> out;
    if (!qCpu || !ctrCpuAll)
        return out;

    PdhCollectQueryData(qCpu);

    DWORD bufSize = 0, itemCount = 0;
    PDH_STATUS s = PdhGetFormattedCounterArrayW(ctrCpuAll, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
    if (s != PDH_MORE_DATA)
        return out;

    std::vector<BYTE> buf(bufSize);
    auto *items = reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM_W>(buf.data());
    s = PdhGetFormattedCounterArrayW(ctrCpuAll, PDH_FMT_DOUBLE, &bufSize, &itemCount, items);
    if (s != ERROR_SUCCESS)
        return out;

    out.reserve(itemCount);
    for (DWORD i = 0; i < itemCount; ++i)
    {
        if (items[i].szName && std::wcscmp(items[i].szName, L"_Total") == 0)
            continue;

        double v = items[i].FmtValue.doubleValue;
        if (!std::isfinite(v))
            v = 0.0;
        if (v < 0.0)
            v = 0.0;
        if (v > 100.0)
            v = 100.0;
        out.push_back(v);
    }
    return out;
}

std::wstring PdhSampleDiskLine()
{
    if (!qDisk)
        return L"disk: — | —";
    double r = 0.0, w = 0.0;
    if (!PdhSampleDiskTotals(r, w))
        return L"disk: — | —";
    std::wstringstream ss;
    ss << L"disk: R " << humanRate(r) << L" | W " << humanRate(w);
    return ss.str();
}

std::wstring PdhSampleNetLine()
{
    if (!qNet)
        return L"net : — | —";
    double s = 0.0, r = 0.0;
    if (!PdhSampleNetTotals(s, r))
        return L"net : — | —";
    std::wstringstream ss;
    ss << L"net : \x2191 " << humanRate(s) << L" | \x2193 " << humanRate(r);
    return ss.str();
}

bool PdhSampleDiskTotals(double &readBps, double &writeBps)
{
    readBps = writeBps = 0.0;
    if (!qDisk || (!ctrDiskRead && !ctrDiskWrite))
        return false;

    PdhCollectQueryData(qDisk);

    PDH_FMT_COUNTERVALUE vR{}, vW{};
    DWORD type = 0;
    if (ctrDiskRead)
        PdhGetFormattedCounterValue(ctrDiskRead, PDH_FMT_DOUBLE, &type, &vR);
    if (ctrDiskWrite)
        PdhGetFormattedCounterValue(ctrDiskWrite, PDH_FMT_DOUBLE, &type, &vW);

    double r = std::isfinite(vR.doubleValue) ? vR.doubleValue : 0.0;
    double w = std::isfinite(vW.doubleValue) ? vW.doubleValue : 0.0;
    if (r < 0)
        r = 0;
    if (w < 0)
        w = 0;
    readBps = r;
    writeBps = w;
    return true;
}

bool PdhSampleNetTotals(double &sentBps, double &recvBps)
{
    sentBps = recvBps = 0.0;
    if (!qNet || netBytesTotal.empty())
        return false;

    PdhCollectQueryData(qNet);

    for (size_t idx = 0; idx < netBytesTotal.size(); ++idx)
    {
        PDH_HCOUNTER h = netBytesTotal[idx];
        if (!h)
            continue;

        DWORD bufSize = 0, itemCount = 0;
        PDH_STATUS s = PdhGetFormattedCounterArrayW(h, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
        if (s != PDH_MORE_DATA)
            continue;

        std::vector<BYTE> buf(bufSize);
        auto *items = reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM_W>(buf.data());
        s = PdhGetFormattedCounterArrayW(h, PDH_FMT_DOUBLE, &bufSize, &itemCount, items);
        if (s != ERROR_SUCCESS)
            continue;

        double sum = 0.0;
        for (DWORD i = 0; i < itemCount; ++i)
        {
            double v = items[i].FmtValue.doubleValue;
            if (!std::isfinite(v) || v < 0)
                v = 0.0;
            sum += v;
        }

        if (idx == 0)
            recvBps = sum;
        else
            sentBps = sum;
    }
    return true;
}
