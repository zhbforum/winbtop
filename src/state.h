#pragma once
#include <vector>
#include <deque>
#include <mutex>
#include <string>
#include "metrics.h"

enum class UiMode
{
    Normal,
    ThemePicker,
    MainMenu,
    Help
};

template <typename T>
class Ring
{
public:
    explicit Ring(size_t cap = 180) : cap_(cap) {}
    void push(T v)
    {
        if (buf_.size() == cap_)
            buf_.pop_front();
        buf_.push_back(v);
    }
    const std::deque<T> &data() const { return buf_; }
    size_t size() const { return buf_.size(); }
    bool empty() const { return buf_.empty(); }

private:
    size_t cap_;
    std::deque<T> buf_;
};

struct DiskStat
{
    std::wstring name;
    double rBps = 0, wBps = 0;
    double ioPct = 0;
};
struct NetStat
{
    std::wstring name;
    double upBps = 0, downBps = 0;
};

struct AppState
{
    UiMode ui = UiMode::Normal;

    std::vector<double> cpuCores;
    MemInfo mem{};
    std::vector<ProcInfo> procs;

    Ring<double> cpuHist{180};
    Ring<double> memHist{180};

    Ring<double> diskR_Hist{180};
    Ring<double> diskW_Hist{180};
    Ring<double> netUp_Hist{180};
    Ring<double> netDn_Hist{180};

    double cpuTotal = 0.0;

    int menuIndex = 0;

    int hz = 5;

    int procSort = 0;
    int procScroll = 0;
    int procIndex = 0;

    std::mutex m;
};
