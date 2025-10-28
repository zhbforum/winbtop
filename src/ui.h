#pragma once
#include <string>
#include <vector>
#include "metrics.h"
#include "state.h"

struct Layout
{
    short cols = 0, rows = 0;
    short left = 1, top = 1, right = 1, bottom = 1;
    short panelWidth = 0;
};

struct TableMetrics
{
    int tableTop; 
    int pageRows; 
};

Layout ComputeLayout();
TableMetrics ComputeTableMetrics(const Layout &L);

std::wstring BuildFrame(
    const Layout &L,
    double cpuUsage,
    const MemInfo &mem,
    const std::vector<ProcInfo> &procs,
    int hz,
    const std::vector<double> &perCoreCpu,
    const std::wstring &netLine,
    const std::wstring &diskLine,
    const std::wstring &netSpark,
    const std::wstring &diskSpark,
    int procSort,
    int procScroll,
    int selectedIndex = -1,
    int totalCount = 0);

std::wstring BuildOverlayMainMenu(const Layout &L, const AppState &st);
std::wstring BuildOverlayThemePicker(const Layout &L, const std::wstring &currentThemeName);
std::wstring BuildOverlayHelp(const Layout &L);
