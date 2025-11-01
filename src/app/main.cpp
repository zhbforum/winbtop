#include <windows.h>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <sstream>
#include <algorithm>

#include "util.h"
#include "ui.h"
#include "state.h"
#include "sampler.h"
#include "ui_graph.h"
#include "pdh_metrics.h"
#include "theme.h"
#include "settings.h"

static Sampler *gSampler = nullptr;
static ThemeManager gThemes;

static bool DebounceKey(DWORD vk, DWORD minMs = 180)
{
    struct Slot
    {
        DWORD vk;
        ULONGLONG t;
    };
    static Slot slots[8] = {};
    ULONGLONG now = GetTickCount64();

    for (auto &s : slots)
    {
        if (s.vk == vk)
        {
            if (now - s.t < minMs)
                return true;
            s.t = now;
            return false;
        }
    }
    for (auto &s : slots)
    {
        if (s.vk == 0)
        {
            s.vk = vk;
            s.t = now;
            return false;
        }
    }
    int idx = 0;
    ULONGLONG best = slots[0].t;
    for (int i = 1; i < 8; ++i)
        if (slots[i].t < best)
        {
            best = slots[i].t;
            idx = i;
        }
    slots[idx] = {vk, now};
    return false;
}

static void HandleInput(AppState &state,
                        int pageRows,
                        int totalCount,
                        bool &running,
                        bool &themeChangedInPicker,
                        bool &resized,
                        bool &uiDirty)
{
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD nEvents = 0;
    if (!GetNumberOfConsoleInputEvents(hIn, &nEvents) || nEvents == 0)
        return;

    std::vector<INPUT_RECORD> recs(nEvents);
    DWORD read = 0;
    if (!ReadConsoleInputW(hIn, recs.data(), nEvents, &read) || read == 0)
        return;

    auto clampSel = [&]()
    {
        if (totalCount <= 0)
        {
            state.procIndex = 0;
            state.procScroll = 0;
            return;
        }

        if (state.procIndex < 0)
            state.procIndex = 0;
        if (state.procIndex >= totalCount)
            state.procIndex = totalCount - 1;

        if (pageRows < 1)
            pageRows = 1;

        int first = state.procScroll;
        int last = first + pageRows - 1;

        if (state.procIndex < first)
            state.procScroll = state.procIndex;
        else if (state.procIndex > last)
            state.procScroll = state.procIndex - (pageRows - 1);

        int maxScroll = std::max(0, totalCount - pageRows);
        state.procScroll = std::clamp(state.procScroll, 0, maxScroll);

        state.procIndex = std::clamp(state.procIndex, state.procScroll, state.procScroll + pageRows - 1);
    };

    for (DWORD i = 0; i < read; ++i)
    {
        const INPUT_RECORD &ev = recs[i];

        if (ev.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            resized = true;
            uiDirty = true;
            continue;
        }

        if (ev.EventType == KEY_EVENT && ev.Event.KeyEvent.bKeyDown)
        {
            const auto &ke = ev.Event.KeyEvent;

            if (ke.wVirtualKeyCode == VK_ESCAPE && !DebounceKey(VK_ESCAPE))
            {
                if (state.ui == UiMode::ThemePicker || state.ui == UiMode::Help)
                    state.ui = UiMode::MainMenu;
                else if (state.ui == UiMode::MainMenu)
                    state.ui = UiMode::Normal;
                else
                    state.ui = UiMode::MainMenu;
                uiDirty = true;
                continue;
            }

            if (ke.wVirtualKeyCode == 'H' && state.ui == UiMode::Normal && !DebounceKey('H'))
            {
                state.ui = UiMode::Help;
                uiDirty = true;
                continue;
            }

            if (state.ui == UiMode::MainMenu)
            {
                const int itemsCount = 3;
                switch (ke.wVirtualKeyCode)
                {
                case VK_UP:
                    state.menuIndex = std::max(0, state.menuIndex - 1);
                    uiDirty = true;
                    break;
                case VK_DOWN:
                    state.menuIndex = std::min(itemsCount - 1, state.menuIndex + 1);
                    uiDirty = true;
                    break;
                case VK_RETURN:
                    if (DebounceKey(VK_RETURN))
                        break;
                    if (state.menuIndex == 0)
                        state.ui = UiMode::ThemePicker;
                    else if (state.menuIndex == 1)
                        state.ui = UiMode::Help;
                    else if (state.menuIndex == 2)
                        running = false;
                    uiDirty = true;
                    break;
                default:
                    break;
                }
                continue;
            }

            if (state.ui == UiMode::ThemePicker)
            {
                switch (ke.wVirtualKeyCode)
                {
                case VK_LEFT:
                case VK_UP:
                    gThemes.Prev();
                    themeChangedInPicker = true;
                    uiDirty = true;
                    break;
                case VK_RIGHT:
                case VK_DOWN:
                    gThemes.Next();
                    themeChangedInPicker = true;
                    uiDirty = true;
                    break;
                case VK_RETURN:
                case VK_ESCAPE:
                    if (!DebounceKey(ke.wVirtualKeyCode))
                    {
                        state.ui = UiMode::MainMenu;
                        uiDirty = true;
                    }
                    break;
                default:
                    break;
                }
                continue;
            }

            if (state.ui == UiMode::Help)
            {
                if ((ke.wVirtualKeyCode == VK_RETURN || ke.wVirtualKeyCode == VK_ESCAPE) && !DebounceKey(ke.wVirtualKeyCode))
                {
                    state.ui = UiMode::MainMenu;
                    uiDirty = true;
                }
                continue;
            }

            switch (ke.wVirtualKeyCode)
            {
            case 'Q':
                running = false;
                break;
            case 'T':
                gThemes.Next();
                uiDirty = true;
                break;
            case 'Y':
                gThemes.Prev();
                uiDirty = true;
                break;

            case VK_F1:
                state.procSort = 1;
                uiDirty = true;
                break;
            case VK_F2:
                state.procSort = 0;
                uiDirty = true;
                break;
            case VK_F3:
                state.procSort = 2;
                uiDirty = true;
                break;
            case VK_F6:
                state.procSort = 3;
                uiDirty = true;
                break;

            case VK_F5:
            {
                static int rateIdx = 1;
                const int rates[] = {2, 5, 10, 20};
                rateIdx = (rateIdx + 1) % (int)(sizeof(rates) / sizeof(rates[0]));
                std::scoped_lock lk(state.m);
                state.hz = rates[rateIdx];
                uiDirty = true;
                break;
            }

            case VK_NEXT:
            {
                int maxRows = std::max(1, pageRows);
                int maxScroll = std::max(0, totalCount - maxRows);

                int oldFirst = state.procScroll;
                state.procScroll = std::min(maxScroll, state.procScroll + maxRows);
                int moved = state.procScroll - oldFirst;

                state.procIndex += moved;
                clampSel();
                uiDirty = true;
                break;
            }
            case VK_PRIOR:
            {
                int maxRows = std::max(1, pageRows);

                int oldFirst = state.procScroll;
                state.procScroll = std::max(0, state.procScroll - maxRows);
                int moved = state.procScroll - oldFirst;

                state.procIndex += moved;
                clampSel();
                uiDirty = true;
                break;
            }
            case VK_DOWN:
                state.procIndex += 1;
                clampSel();
                uiDirty = true;
                break;
            case VK_UP:
                state.procIndex -= 1;
                clampSel();
                uiDirty = true;
                break;
            case VK_HOME:
                state.procIndex = 0;
                clampSel();
                uiDirty = true;
                break;
            case VK_END:
                state.procIndex = std::max(0, totalCount - 1);
                clampSel();
                uiDirty = true;
                break;

            case 'M':
                state.ui = UiMode::MainMenu;
                uiDirty = true;
                break;
            default:
                break;
            }
        }
        else if (ev.EventType == MOUSE_EVENT && state.ui == UiMode::Normal)
        {
            const auto &me = ev.Event.MouseEvent;
            if (me.dwEventFlags == MOUSE_WHEELED)
            {
                SHORT delta = (SHORT)HIWORD(me.dwButtonState);
                bool page = (me.dwControlKeyState & SHIFT_PRESSED) != 0;
                int stepLines = page ? pageRows : 3;

                if (pageRows < 1)
                    pageRows = 1;
                int maxScroll = std::max(0, totalCount - pageRows);

                int oldScroll = state.procScroll;
                if (delta > 0)
                    state.procScroll = std::max(0, state.procScroll - stepLines);
                else if (delta < 0)
                    state.procScroll = std::min(maxScroll, state.procScroll + stepLines);

                int moved = state.procScroll - oldScroll;
                state.procIndex += moved;

                clampSel();
                uiDirty = true;
            }
        }
    }
}

int wmain()
{
    if (!InitConsole())
        return 1;

    Settings cfg;
    LoadSettings(cfg);

    const std::wstring themesDir = ResolveThemesDir();
    if (!gThemes.LoadDir(themesDir))
    {
        wprintf(L"[!] Не удалось загрузить темы из: %s\n", themesDir.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }
    else
    {
        if (!cfg.themeName.empty())
            gThemes.SetByName(cfg.themeName);
        auto names = gThemes.Names();
        wprintf(L"[+] Загружено %zu тем из %s:\n", names.size(), themesDir.c_str());
        for (auto &n : names)
            wprintf(L"   %s\n", n.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    ClearScreen();
    std::atexit(+[]
                { ShutdownConsole(); });

    AppState state;
    if (cfg.hz > 0)
        state.hz = cfg.hz;

    Sampler sampler(state);
    gSampler = &sampler;
    sampler.start();

    bool running = true;

    UiMode prevUi = UiMode::Normal;
    ULONGLONG lastModalBaseRedraw = 0;
    std::wstring lastThemeName = gThemes.Current().name;

    auto draw_base = [&](const Layout &L,
                         double cpuUsage,
                         const MemInfo &mem,
                         const std::vector<ProcInfo> &procs,
                         int hz,
                         const std::vector<double> &perCore,
                         const std::wstring &netLine,
                         const std::wstring &diskLine,
                         const std::wstring &netSpark,
                         const std::wstring &diskSpark,
                         int procSort,
                         int procScroll,
                         int selectedIndex,
                         int totalCount)
    {
        std::wstring frame = BuildFrame(
            L, cpuUsage, mem, procs, hz, perCore,
            netLine, diskLine, netSpark, diskSpark,
            procSort, procScroll, selectedIndex, totalCount);

        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        MoveCursor(1, 1);
        DWORD w;
        WriteConsoleW(hout, frame.c_str(), (DWORD)frame.size(), &w, nullptr);
    };

    constexpr int UI_FPS = 60;
    const auto uiFrameDur = std::chrono::duration<double>(1.0 / UI_FPS);

    while (running)
    {
        auto start = std::chrono::steady_clock::now();

        Layout L = ComputeLayout();
        auto tm = ComputeTableMetrics(L);
        const int pageRows = tm.pageRows;

        int procCount = 0;
        {
            std::scoped_lock lk(state.m);
            procCount = (int)state.procs.size();
        }

        bool themeChangedInPicker = false;
        bool resized = false;
        bool uiDirty = false;
        HandleInput(state, pageRows, procCount, running, themeChangedInPicker, resized, uiDirty);

        bool needBase = uiDirty || resized || (state.ui == UiMode::Normal);
        ULONGLONG nowTick = GetTickCount64();

        if (state.ui != UiMode::Normal)
        {
            if (themeChangedInPicker)
                needBase = true;
            else if (nowTick - lastModalBaseRedraw >= 500)
                needBase = true;
        }
        if (prevUi != state.ui)
            needBase = true;

        double cpuUsage = 0.0;
        MemInfo mem{};
        std::vector<ProcInfo> procs;
        std::vector<double> perCore;
        std::wstring diskLine, netLine;
        std::wstring diskSpark, netSpark;

        if (needBase)
        {
            {
                std::scoped_lock lk(state.m);
                cpuUsage = state.cpuTotal;
                mem = state.mem;
                perCore = state.cpuCores;
                procs = state.procs;

                diskSpark = spark_braille(state.diskR_Hist.data(), 24);
                netSpark = spark_braille(state.netUp_Hist.data(), 24);
            }
            diskLine = PdhSampleDiskLine();
            netLine = PdhSampleNetLine();

            draw_base(L, cpuUsage, mem, procs, state.hz, perCore,
                      netLine, diskLine, netSpark, diskSpark,
                      state.procSort, state.procScroll, state.procIndex, (int)procs.size());

            if (state.ui != UiMode::Normal)
                lastModalBaseRedraw = nowTick;
        }

        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD w;

        if (state.ui == UiMode::MainMenu)
        {
            std::wstring overlay = BuildOverlayMainMenu(L, state);
            WriteConsoleW(hout, overlay.c_str(), (DWORD)overlay.size(), &w, nullptr);
        }
        else if (state.ui == UiMode::ThemePicker)
        {
            std::wstring cur = gThemes.Current().name;
            if (cur != lastThemeName)
                lastThemeName = cur;
            std::wstring overlay = BuildOverlayThemePicker(L, cur);
            WriteConsoleW(hout, overlay.c_str(), (DWORD)overlay.size(), &w, nullptr);
        }
        else if (state.ui == UiMode::Help)
        {
            std::wstring overlay = BuildOverlayHelp(L);
            WriteConsoleW(hout, overlay.c_str(), (DWORD)overlay.size(), &w, nullptr);
        }

        prevUi = state.ui;

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < uiFrameDur)
            std::this_thread::sleep_for(uiFrameDur - elapsed);
    }

    Settings outCfg;
    outCfg.themeName = gThemes.Current().name;
    {
        std::scoped_lock lk(state.m);
        outCfg.hz = state.hz;
    }
    SaveSettings(outCfg);

    sampler.stop();
    return 0;
}
