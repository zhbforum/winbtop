#include "sampler.h"

#include <thread>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "metrics.h"
#include "metrics_process.h"
#include "pdh_metrics.h"

void Sampler::start()
{
    if (on.exchange(true))
        return;
    th = std::thread(&Sampler::run, this);
}

void Sampler::stop()
{
    on = false;
    if (th.joinable())
        th.join();
}

void Sampler::run()
{
    PdhInit();

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const int logicalCores = (int)si.dwNumberOfProcessors;

    CpuTimes prevSys{}, currSys{};
    GetSystemCpuTimes(prevSys);

    struct PrevCpu
    {
        ULONGLONG k = 0, u = 0;
    };
    std::unordered_map<DWORD, PrevCpu> prevByPid;

    std::unordered_map<DWORD, std::wstring> cacheUser;
    std::unordered_map<DWORD, std::wstring> cacheCmd;
    std::unordered_map<DWORD, std::wstring> cacheName;

    while (on)
    {
        auto t0 = std::chrono::steady_clock::now();

        int localHz = 5;
        {
            std::scoped_lock lk(st.m);
            localHz = std::max(1, st.hz);
        }
        const double elapsedSec = 1.0 / (double)localHz;

        GetSystemCpuTimes(currSys);
        double cpuTotal = CalcCpuUsage(prevSys, currSys);
        prevSys = currSys;

        MemInfo mem = GetMemoryInfo();
        auto perCore = PdhSamplePerCoreCpu();

        std::vector<ProcRaw> raw;
        SnapshotProcesses(raw);

        const long double TICKS_PER_SEC = 10000000.0L;

        std::vector<ProcInfo> procs;
        procs.reserve(raw.size());

        std::unordered_set<DWORD> alive;
        alive.reserve(raw.size());

        for (const auto &r : raw)
        {
            alive.insert(r.pid);

            ProcInfo p{};
            p.pid = r.pid;
            p.threads = r.threads;
            p.workingSet = r.workingSet;

            if (!r.imageName.empty())
                p.name = r.imageName;
            else
            {
                auto itN = cacheName.find(p.pid);
                if (itN != cacheName.end())
                    p.name = itN->second;
                else
                {
                    p.name = GetProcessBaseNameLazy(p.pid);
                    cacheName[p.pid] = p.name;
                }
            }

            PrevCpu now{r.kernel, r.user};
            auto itPrev = prevByPid.find(p.pid);
            double pct = 0.0;
            if (itPrev != prevByPid.end())
            {
                long double dk = (long double)(now.k - itPrev->second.k);
                long double du = (long double)(now.u - itPrev->second.u);
                long double busy = dk + du;
                pct = (double)(busy / (elapsedSec * (double)TICKS_PER_SEC * (double)logicalCores) * 100.0);
            }
            prevByPid[p.pid] = now;
            if (pct < 0)
                pct = 0;
            if (pct > 999.9)
                pct = 999.9;
            p.cpu_percent = pct;

            procs.emplace_back(std::move(p));
        }

        std::sort(procs.begin(), procs.end(),
                  [](const ProcInfo &a, const ProcInfo &b)
                  { return a.workingSet > b.workingSet; });

        const size_t FILL_LIMIT = std::min<size_t>(procs.size(), 120);
        for (size_t i = 0; i < FILL_LIMIT; ++i)
        {
            auto &p = procs[i];

            if (p.user.empty())
            {
                auto itU = cacheUser.find(p.pid);
                if (itU != cacheUser.end())
                    p.user = itU->second;
                else
                {
                    std::wstring u = GetProcessUserLazy(p.pid);
                    if (!u.empty())
                    {
                        cacheUser[p.pid] = u;
                        p.user = std::move(u);
                    }
                }
            }
            if (p.cmdline.empty())
            {
                auto itC = cacheCmd.find(p.pid);
                if (itC != cacheCmd.end())
                    p.cmdline = itC->second;
                else
                {
                    std::wstring cmd = GetProcessCommandLineLazy(p.pid);
                    if (!cmd.empty())
                    {
                        cacheCmd[p.pid] = cmd;
                        p.cmdline = std::move(cmd);
                    }
                }
            }
        }

        {
            std::scoped_lock lk(st.m);
            st.cpuTotal = cpuTotal;
            st.cpuCores = std::move(perCore);
            st.mem = mem;
            st.procs = std::move(procs);
            st.cpuHist.push(cpuTotal);
            st.memHist.push(mem.percent);
        }

        for (auto it = prevByPid.begin(); it != prevByPid.end();)
            it = (alive.count(it->first) ? std::next(it) : prevByPid.erase(it));
        for (auto it = cacheUser.begin(); it != cacheUser.end();)
            it = (alive.count(it->first) ? std::next(it) : cacheUser.erase(it));
        for (auto it = cacheCmd.begin(); it != cacheCmd.end();)
            it = (alive.count(it->first) ? std::next(it) : cacheCmd.erase(it));
        for (auto it = cacheName.begin(); it != cacheName.end();)
            it = (alive.count(it->first) ? std::next(it) : cacheName.erase(it));

        auto frame = std::chrono::duration<double>(1.0 / (double)localHz);
        auto elapsed = std::chrono::steady_clock::now() - t0;
        if (elapsed < frame)
            std::this_thread::sleep_for(frame - elapsed);
    }

    PdhShutdown();
}
