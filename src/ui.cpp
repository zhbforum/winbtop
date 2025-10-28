#include "ui.h"
#include "util.h"
#include "theme.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <windows.h>
#include <winreg.h>

static constexpr wchar_t H = L'\u2500';
static constexpr wchar_t V = L'\u2502';
static constexpr wchar_t TL = L'\u250C';
static constexpr wchar_t TR = L'\u2510';
static constexpr wchar_t BL = L'\u2514';
static constexpr wchar_t BR = L'\u2518';

static void put(std::wstring &f, short row, short col, std::wstring_view s)
{
    std::wstringstream ss;
    ss << L"\x1b[" << row << L";" << col << L"H" << s;
    f += ss.str();
}

static void put_eol_bg(std::wstring &f, short row, short col, const std::wstring &s, const Rgb &bg)
{
    const std::wstring rst = RST();
    std::wstring body = s;
    if (body.size() >= rst.size() && body.rfind(rst) == body.size() - rst.size())
        body.erase(body.size() - rst.size());

    std::wstringstream ss;
    ss << L"\x1b[" << row << L';' << col << L'H'
       << bg24(bg) << body
       << bg24(bg) << L"\x1b[K"
       << RST();
    f += ss.str();
}

static void FillRectBG(std::wstring &f, short top, short left, short height, short width, const Rgb &color)
{
    if (height <= 0 || width <= 0)
        return;
    const std::wstring row = bg24(color) + std::wstring(width, L' ') + RST();
    for (short r = 0; r < height; ++r)
        put(f, (short)(top + r), left, row);
}

static std::wstring apply_bg(std::wstring s, const Rgb &bg)
{
    const std::wstring rst = RST();
    const std::wstring b = bg24(bg);
    size_t pos = 0;
    while ((pos = s.find(rst, pos)) != std::wstring::npos)
    {
        s.insert(pos + rst.size(), b);
        pos += (size_t)rst.size() + (size_t)b.size();
    }
    return b + s + RST();
}

static void Box(std::wstring &f, short top, short left, short height, short width,
                std::wstring title, const Rgb &bgColor, const Rgb *titleColor = nullptr)
{
    std::wstring line(width, H);
    if (!title.empty() && (int)title.size() + 2 < width)
    {
        std::wstring deco = L" " + title + L" ";
        for (int i = 1, j = 0; i < width - 1 && j < (int)deco.size(); ++i)
            line[i] = deco[j++];
    }

    std::wstring topLine, mid, bottomLine;
    topLine.push_back(TL);
    topLine.append(line.begin() + 1, line.begin() + width - 1);
    topLine.push_back(TR);
    mid.push_back(V);
    mid.append(width - 2, L' ');
    mid.push_back(V);
    bottomLine.push_back(BL);
    bottomLine.append(line.begin() + 1, line.begin() + width - 1);
    bottomLine.push_back(BR);

    const std::wstring frameCol = fg24(ActiveTheme().frame);
    const std::wstring bg = bg24(bgColor);

    put(f, top, left, bg + frameCol + topLine + RST());
    for (short r = top + 1; r <= top + height - 2; ++r)
        put(f, r, left, bg + frameCol + mid + RST());
    put(f, (short)(top + height - 1), left, bg + frameCol + bottomLine + RST());

    if (!title.empty())
    {
        const std::wstring tcol = titleColor ? fg24(*titleColor) : col_hdr();
        put(f, top, (short)(left + 2), bg + tcol + title + RST());
    }
}

static void FilledBox(std::wstring &f, short top, short left, short height, short width,
                      std::wstring title, const Rgb &innerBg, const Rgb *titleColor = nullptr)
{
    FillRectBG(f, (short)(top + 1), (short)(left + 1), (short)(height - 2), (short)(width - 2), innerBg);
    Box(f, top, left, height, width, std::move(title), innerBg, titleColor);
}

static void ProgressBar(std::wstring &f, short row, short col, int width, double percent)
{
    if (width <= 0)
        return;
    percent = std::clamp(percent, 0.0, 100.0);
    int filled = (int)std::round((percent / 100.0) * width);
    const auto &t = ActiveTheme();
    std::wstringstream ss;
    ss << L"\x1b[" << row << L";" << col << L"H" << bg24(t.meter_bg) << std::wstring(width, L' ') << RST();
    ss << L"\x1b[" << row << L";" << col << L"H";
    for (int i = 0; i < filled; ++i)
    {
        double k = (filled <= 1) ? 1.0 : (double)i / (filled - 1);
        int r = (int)std::round(t.barLo.r + (t.barHi.r - t.barLo.r) * k);
        int g = (int)std::round(t.barLo.g + (t.barHi.g - t.barLo.g) * k);
        int b = (int)std::round(t.barLo.b + (t.barHi.b - t.barLo.b) * k);
        ss << bg24(r, g, b) << L' ';
    }
    ss << RST();
    f += ss.str();
}

Layout ComputeLayout()
{
    COORD s = GetConsoleSize();
    Layout L;
    L.cols = s.X;
    L.rows = s.Y;
    L.left = 2;
    L.top = 1;
    L.right = L.cols - 2;
    L.bottom = L.rows - 1;

    const int GAP = 1;
    const int inner = (L.right - L.left + 1);
    L.panelWidth = (short)((inner - GAP) / 2);
    if (L.panelWidth < 40)
        L.panelWidth = (short)(L.cols - 4);
    return L;
}

TableMetrics ComputeTableMetrics(const Layout &L)
{
    const int boxHeight = 8;
    const int tableTop = 1 + boxHeight + 1;
    const int pageRows = std::max(1, (int)L.rows - tableTop - 3);
    return {tableTop, pageRows};
}

static std::wstring Ellipsis(std::wstring s, size_t w)
{
    if (w == 0)
        return L"";
    if (s.size() <= w)
        return PadRight(std::move(s), w);
    if (w <= 1)
        return s.substr(0, w);
    return s.substr(0, w - 1) + L"…";
}

static std::wstring MiddleEllipsis(const std::wstring &s, size_t w)
{
    if (w == 0)
        return L"";
    if (s.size() <= w)
        return PadRight(s, w);
    if (w <= 1)
        return s.substr(0, w);
    size_t left = (w - 1) / 2;
    size_t right = (w - 1) - left;
    return s.substr(0, left) + L"…" + s.substr(s.size() - right);
}

static std::wstring HeaderLine(int pidW, int nameW, int cmdW, int thW, int userW, int memW, int cpuW)
{
    std::wstringstream hdr;
    hdr << col_hdr()
        << std::setw(pidW) << L"Pid" << L" "
        << PadRight(L"Program", nameW) << L" ";
    if (cmdW > 0)
        hdr << PadRight(L"Command", cmdW) << L" ";
    hdr << PadRight(L"Threads", thW) << L" "
        << PadRight(L"User", userW) << L" "
        << PadRight(L"MemB", memW) << L" "
        << PadRight(L"Cpu%", cpuW);
    return hdr.str();
}

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
    int selectedIndex,
    int totalCount)
{
    (void)totalCount;

    std::wstring f;
    f.reserve(L.rows * (L.cols + 8));

    f += bg24(ActiveTheme().bg) + L"\x1b[2J\x1b[H" + RST();

    FillRectBG(f, 1, 1, L.rows, (short)(L.cols), ActiveTheme().panel);
    FillRectBG(f, 2, 2, (short)(L.rows - 2), (short)(L.cols - 1), ActiveTheme().bg);

    wchar_t cpuName[256] = L"CPU";
    DWORD sz = sizeof(cpuName);
    RegGetValueW(HKEY_LOCAL_MACHINE,
                 L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                 L"ProcessorNameString",
                 RRF_RT_REG_SZ, nullptr, &cpuName, &sz);
    ULONGLONG ms = GetTickCount64();
    int days = (int)(ms / (1000ull * 60 * 60 * 24));
    int hrs = (int)((ms / (1000ull * 60 * 60)) % 24);

    std::wstringstream title;
    title << L"winbtop — " << cpuName << L" — Uptime: "
          << days << L"d " << hrs << L"h  — Q quit · F1 cpu% · F2 mem · F3 pid · F6 name · F5 Hz · Esc menu · H help";
    SetTitle(title.str());

    short row = 1;
    short col1 = L.left;
    const int GAP = 1;
    const int inner = (L.right - L.left + 1);

    short wLeft = L.panelWidth;
    short wRight = (short)(inner - GAP - wLeft);
    short col2 = (short)(col1 + wLeft + GAP);

    const Rgb innerCpuBg = ActiveTheme().overlay;
    FilledBox(f, row, col1, 8, wLeft, L" CPU ", innerCpuBg, &ActiveTheme().box_cpu);
    {
        std::wstringstream ss;
        ss << L"Usage: " << std::fixed << std::setprecision(1) << cpuUsage << L"%   (" << hz << L" Hz)";
        put(f, row + 2, col1 + 2, apply_bg(col_text() + ss.str(), innerCpuBg));
        ProgressBar(f, row + 3, col1 + 2, (int)wLeft - 4, cpuUsage);

        const int barW = 12, colGap = 18;
        int perRow = std::max(1, ((int)wLeft - 6) / colGap);
        short r = (short)(row + 5), c = (short)(col1 + 2);
        for (size_t i = 0; i < perCoreCpu.size(); ++i)
        {
            std::wstringstream lab;
            lab << L"C" << i << L": ";
            put(f, r, c, apply_bg(col_text() + lab.str(), innerCpuBg));
            ProgressBar(f, r, (short)(c + 4), barW, perCoreCpu[i]);
            c = (short)(c + colGap);
            if (((i + 1) % perRow) == 0)
            {
                r++;
                c = (short)(col1 + 2);
            }
            if (r >= row + 7)
                break;
        }
    }

    const Rgb innerMemBg = ActiveTheme().overlay;
    FilledBox(f, row, col2, 8, wRight, L" Memory ", innerMemBg, &ActiveTheme().box_mem);
    {
        put(f, row + 2, col2 + 2, apply_bg(col_text() + L"Total: " + FormatBytesULONGLONG(mem.total), innerMemBg));
        put(f, row + 3, col2 + 2, apply_bg(col_text() + L"Used : " + FormatBytesULONGLONG(mem.used), innerMemBg));
        put(f, row + 4, col2 + 2, apply_bg(col_text() + L"Avail: " + FormatBytesULONGLONG(mem.avail), innerMemBg));
        ProgressBar(f, row + 5, col2 + 2, (int)wRight - 4, mem.percent);

        auto lineWithSpark = [&](const std::wstring &line, const std::wstring &spark, int width)
        {
            if (width <= 12 || spark.empty())
                return line;
            int maxText = std::max(0, width - 2 - (int)spark.size());
            std::wstring text = line;
            if ((int)text.size() > maxText)
            {
                text = text.substr(0, std::max(0, maxText - 1));
                text.push_back(L'…');
            }
            return text + L"  " + spark;
        };

        const int innerWidth = (int)wRight - 4;
        std::wstring diskLn = lineWithSpark(diskLine, diskSpark, innerWidth);
        std::wstring netLn = lineWithSpark(netLine, netSpark, innerWidth);

        put(f, row + 6, col2 + 2, apply_bg(col_text() + diskLn, innerMemBg));
        put(f, row + 7, col2 + 2, apply_bg(col_text() + netLn, innerMemBg));
    }

    auto tm = ComputeTableMetrics(L);
    short tableTop = (short)tm.tableTop;
    const Rgb innerProcBg = ActiveTheme().overlay;
    FilledBox(f, tableTop, 2, (short)(L.rows - tableTop - 1), (short)(L.cols - 4), L" Top processes ", innerProcBg, &ActiveTheme().box_proc);
    {
        const short innerLeftCol = 3;
        short innerRow = tableTop + 2;

        const int pidW = 5, thW = 7, memW = 11, cpuW = 6;
        const int boxWidth = (L.cols - 4);
        const int innerWidth = boxWidth - 2;
        const int maxRows = tm.pageRows;

        const int nameW_min = 10, userW_min = 10, cmdW_min = 15;
        const int seps_no_cmd = 5, seps_cmd = 6;
        const int fixed = pidW + thW + memW + cpuW;

        const int flex_no_cmd = innerWidth - fixed - seps_no_cmd;
        const int flex_cmd = innerWidth - fixed - seps_cmd;
        const bool canShowCmd = (flex_cmd >= (nameW_min + userW_min + cmdW_min));

        std::vector<ProcInfo> sorted = procs;
        switch (procSort)
        {
        case 1:
            std::sort(sorted.begin(), sorted.end(), [](auto &a, auto &b)
                      { return a.cpu_percent > b.cpu_percent; });
            break;
        case 2:
            std::sort(sorted.begin(), sorted.end(), [](auto &a, auto &b)
                      { return a.pid < b.pid; });
            break;
        case 3:
            std::sort(sorted.begin(), sorted.end(), [](auto &a, auto &b)
                      { return a.name < b.name; });
            break;
        default:
            std::sort(sorted.begin(), sorted.end(), [](auto &a, auto &b)
                      { return a.workingSet > b.workingSet; });
            break;
        }

        int maxScroll = std::max(0, (int)sorted.size() - maxRows);
        int first = std::clamp(procScroll, 0, maxScroll);
        int sel = selectedIndex;

        auto memCol = fg24(ActiveTheme().barHi);

        if (!canShowCmd)
        {
            int flex = std::max(0, flex_no_cmd);
            int nameW = std::max(nameW_min, flex / 2);
            int userW = std::max(userW_min, flex - nameW);
            if (nameW + userW > flex)
                userW = std::max(0, flex - nameW);

            put_eol_bg(f, innerRow++, innerLeftCol,
                       apply_bg(HeaderLine(pidW, nameW, 0, thW, userW, memW, cpuW), innerProcBg),
                       innerProcBg);

            int drawn = 0;
            for (int i = first; i < (int)sorted.size() && drawn < maxRows; ++i, ++drawn)
            {
                const auto &p = sorted[i];

                bool selected = (i == sel);

                if (selected)
                {
                    std::wstringstream s;
                    s << std::setw(pidW) << p.pid << L" "
                      << Ellipsis(p.name, nameW) << L" "
                      << std::setw(thW) << p.threads << L" "
                      << Ellipsis(p.user, userW) << L" "
                      << PadRight(FormatBytesULONGLONG((ULONGLONG)p.workingSet), memW) << L" "
                      << std::setw(cpuW) << std::fixed << std::setprecision(1) << p.cpu_percent;
                    put_eol_bg(f, innerRow++, innerLeftCol, fg24(ActiveTheme().sel_fg) + s.str(), ActiveTheme().sel_bg);
                    continue;
                }

                std::wstringstream ln;
                ln << col_hdr() << std::setw(pidW) << p.pid << RST() << L" "
                   << col_text() << Ellipsis(p.name, nameW) << RST() << L" "
                   << col_hdr() << std::setw(thW) << p.threads << RST() << L" "
                   << col_dim() << Ellipsis(p.user, userW) << RST() << L" "
                   << memCol << PadRight(FormatBytesULONGLONG((ULONGLONG)p.workingSet), memW) << RST() << L" ";
                const auto col = (p.cpu_percent > 80) ? col_crit() : (p.cpu_percent > 50) ? col_warn()
                                                                                          : col_ok();
                ln << col << std::setw(cpuW) << std::fixed << std::setprecision(1) << p.cpu_percent << RST();
                put_eol_bg(f, innerRow++, innerLeftCol, apply_bg(ln.str(), innerProcBg), innerProcBg);
            }
        }
        else
        {
            int flex = std::max(0, flex_cmd);
            int nameW = std::max(nameW_min, flex / 5);
            int cmdW = std::max(cmdW_min, (flex * 3) / 5);
            int userW = std::max(userW_min, flex - nameW - cmdW);

            int over = nameW + cmdW + userW - flex;
            if (over > 0)
            {
                int cut = std::min(over, userW - userW_min);
                userW -= cut;
                over -= cut;
            }
            if (over > 0)
            {
                int cut = std::min(over, cmdW - cmdW_min);
                cmdW -= cut;
                over -= cut;
            }
            if (over > 0)
                nameW = std::max(0, nameW - over);

            put_eol_bg(f, innerRow++, innerLeftCol,
                       apply_bg(HeaderLine(pidW, nameW, cmdW, thW, userW, memW, cpuW), innerProcBg),
                       innerProcBg);

            for (int i = first; i < (int)sorted.size() && innerRow < L.rows - 2; ++i)
            {
                const auto &p = sorted[i];

                const std::wstring cmdToShow =
                    (p.cmdline.empty() ? p.name : p.cmdline);

                bool selected = (i == sel);

                if (selected)
                {
                    std::wstringstream s;
                    s << std::setw(pidW) << p.pid << L" "
                      << Ellipsis(p.name, nameW) << L" "
                      << MiddleEllipsis(cmdToShow, cmdW) << L" "
                      << std::setw(thW) << p.threads << L" "
                      << Ellipsis(p.user, userW) << L" "
                      << PadRight(FormatBytesULONGLONG((ULONGLONG)p.workingSet), memW) << L" "
                      << std::setw(cpuW) << std::fixed << std::setprecision(1) << p.cpu_percent;
                    put_eol_bg(f, innerRow++, innerLeftCol, fg24(ActiveTheme().sel_fg) + s.str(), ActiveTheme().sel_bg);
                    continue;
                }

                std::wstringstream ln;
                ln << col_hdr() << std::setw(pidW) << p.pid << RST() << L" "
                   << col_text() << Ellipsis(p.name, nameW) << RST() << L" "
                   << col_dim() << MiddleEllipsis(cmdToShow, cmdW) << RST() << L" "
                   << col_hdr() << std::setw(thW) << p.threads << RST() << L" "
                   << col_dim() << Ellipsis(p.user, userW) << RST() << L" "
                   << memCol << PadRight(FormatBytesULONGLONG((ULONGLONG)p.workingSet), memW) << RST() << L" ";
                const auto col = (p.cpu_percent > 80) ? col_crit() : (p.cpu_percent > 50) ? col_warn()
                                                                                          : col_ok();
                ln << col << std::setw(cpuW) << std::fixed << std::setprecision(1) << p.cpu_percent << RST();
                put_eol_bg(f, innerRow++, innerLeftCol, apply_bg(ln.str(), innerProcBg), innerProcBg);
            }
        }
    }

    put_eol_bg(f, L.rows - 1, 2,
               apply_bg(
                   col_dim() + L"Q " + col_accent() + L"quit  " +
                       col_dim() + L"F1 " + col_accent() + L"cpu%  " +
                       col_dim() + L"F2 " + col_accent() + L"mem  " +
                       col_dim() + L"F3 " + col_accent() + L"pid  " +
                       col_dim() + L"F6 " + col_accent() + L"name  " +
                       col_dim() + L"F5 " + col_accent() + L"Hz  " +
                       col_dim() + L"PgUp/PgDn " + col_accent() + L"scroll  " +
                       col_dim() + L"Esc/M " + col_accent() + L"menu  " +
                       col_dim() + L"H " + col_accent() + L"help",
                   ActiveTheme().bg),
               ActiveTheme().bg);

    return f;
}

std::wstring BuildOverlayMainMenu(const Layout &L, const AppState &st)
{
    const std::wstring title = L"BTOP++ for Windows";
    const short w = (short)std::max<int>(40, (int)title.size() + 8);
    const short h = 12;
    short top = (short)std::max<short>(1, (L.rows - h) / 2);
    short left = (short)std::max<short>(2, (L.cols - w) / 2);

    std::wstring f;
    FillRectBG(f, top, left, h, w, ActiveTheme().overlay);
    Box(f, top, left, h, w, L" Menu ", ActiveTheme().overlay);

    const std::wstring items[3] = {L"Options", L"Help", L"Quit"};
    short listTop = (short)(top + 2);
    short listLeft = (short)(left + 2);
    for (int i = 0; i < 3; ++i)
    {
        const bool sel = (i == st.menuIndex);
        std::wstring line = sel
                                ? (bg24(ActiveTheme().sel_bg) + fg24(ActiveTheme().sel_fg) + L"> " + items[i] + RST())
                                : (col_text() + L"  " + items[i] + RST());
        put(f, (short)(listTop + i), listLeft, apply_bg(line, ActiveTheme().overlay));
    }

    short infoLeft = (short)(left + w / 2);
    int rightW = (left + w - 2) - infoLeft;
    rightW = std::max(rightW, 10);

    auto clip = [&](std::wstring s)
    { return Ellipsis(std::move(s), (size_t)rightW); };

    put(f, (short)(top + 2), infoLeft, apply_bg(col_hdr() + clip(title), ActiveTheme().overlay));
    put(f, (short)(top + 3), infoLeft, apply_bg(col_dim() + clip(L"Description"), ActiveTheme().overlay));
    put(f, (short)(top + 4), infoLeft,
        apply_bg(col_text() + clip(L"Theme & colors. Pick visual style that suits you."), ActiveTheme().overlay));

    const int innerW = w - 4;
    const std::wstring hint_long = L"[↑/↓] select   [Enter] open   [Esc] close";
    const std::wstring hint_short = L"↑/↓  Enter  Esc";
    std::wstring hint = ((int)hint_long.size() <= innerW) ? hint_long
                                                          : Ellipsis(hint_short, (size_t)innerW);

    put(f, (short)(top + h - 2), (short)(left + 2),
        apply_bg(col_dim() + hint, ActiveTheme().overlay));

    return f;
}

std::wstring BuildOverlayThemePicker(const Layout &L, const std::wstring &currentThemeName)
{
    const std::wstring title = L"Options — Theme";
    const short w = (short)std::max<int>(36, (int)title.size() + 6);
    const short h = 7;
    short top = (short)std::max<short>(1, (L.rows - h) / 2);
    short left = (short)std::max<short>(2, (L.cols - w) / 2);

    std::wstring f;
    FillRectBG(f, top, left, h, w, ActiveTheme().overlay);
    Box(f, top, left, h, w, L" Options ", ActiveTheme().overlay);

    put(f, (short)(top + 1), (short)(left + (w - (short)title.size()) / 2),
        apply_bg(col_hdr() + title, ActiveTheme().overlay));

    const int innerW = w - 2;
    const int padL = 3;
    const int textW = innerW - padL - 1;

    const std::wstring prefix = L"Theme: ";
    int maxNameW = std::max(0, textW - (int)prefix.size());
    std::wstring nameTrim = MiddleEllipsis(currentThemeName, (size_t)maxNameW);

    put(f, (short)(top + 3), (short)(left + padL),
        apply_bg(col_text() + prefix + col_accent() + nameTrim, ActiveTheme().overlay));

    auto fits = [&](const std::wstring &s)
    { return (int)s.size() + 2 <= textW; };
    const std::wstring hint_long = L"[←/↑] prev   [→/↓] next   [Enter/Esc] back";
    const std::wstring hint_mid = L"←/↑ prev  →/↓ next  Enter/Esc";
    const std::wstring hint_short = L"←/↑  →/↓  Enter/Esc";
    const std::wstring hint_ascii = L"Up/Down  Enter/Esc";

    std::wstring hint;
    if (fits(hint_long))
        hint = hint_long;
    else if (fits(hint_mid))
        hint = hint_mid;
    else if (fits(hint_short))
        hint = hint_short;
    else
        hint = Ellipsis(hint_ascii, (size_t)std::max(0, textW));

    put(f, (short)(top + 5), (short)(left + padL),
        apply_bg(col_dim() + hint, ActiveTheme().overlay));

    return f;
}

std::wstring BuildOverlayHelp(const Layout &L)
{
    const short w = (short)std::min<int>(L.cols - 8, 78);
    const short h = (short)std::min<int>(L.rows - 6, 18);
    short top = (short)std::max<short>(1, (L.rows - h) / 2);
    short left = (short)std::max<short>(2, (L.cols - w) / 2);

    std::wstring f;
    FillRectBG(f, top, left, h, w, ActiveTheme().overlay);
    Box(f, top, left, h, w, L" Help ", ActiveTheme().overlay);

    short r = (short)(top + 2);
    short c = (short)(left + 2);

    auto line = [&](std::wstring k, std::wstring d)
    {
        std::wstringstream ss;
        ss << col_accent() << PadRight(std::move(k), 10) << RST() << L"  " << col_text() << d << RST();
        put_eol_bg(f, r++, c, apply_bg(ss.str(), ActiveTheme().overlay), ActiveTheme().overlay);
    };

    put(f, r++, c, apply_bg(col_hdr() + L"Keys — Description", ActiveTheme().overlay));
    r++;

    line(L"Q", L"Quit program");
    line(L"Esc / m", L"Open/Close menu");
    line(L"H", L"Show this help");
    line(L"F1 / F2 / F3", L"Sort by CPU% / MEM / PID");
    line(L"F6", L"Sort by NAME");
    line(L"F5", L"Cycle update Hz");
    line(L"PgUp/PgDn", L"Scroll processes");
    line(L"↑/↓/Home/End", L"Navigation");

    r++;
    put(f, (short)(top + h - 2), (short)(left + 2),
        apply_bg(col_dim() + L"[Esc/Enter] back", ActiveTheme().overlay));
    return f;
}
