#include "theme.h"
#include "util.h"

#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>

static Theme g_theme;

const Theme &ActiveTheme() { return g_theme; }
void SetActiveTheme(const Theme &t) { g_theme = t; }

std::wstring col_ok() { return fg24(ActiveTheme().barLo); }
std::wstring col_warn() { return fg24(255, 210, 120); }
std::wstring col_crit() { return fg24(255, 120, 120); }
std::wstring col_dim() { return fg24(ActiveTheme().dim); }
std::wstring col_hdr() { return fg24(ActiveTheme().hdr); }
std::wstring col_text() { return fg24(ActiveTheme().text); }
std::wstring col_accent() { return fg24(ActiveTheme().accent); }

std::wstring col_box_cpu() { return fg24(ActiveTheme().box_cpu); }
std::wstring col_box_mem() { return fg24(ActiveTheme().box_mem); }
std::wstring col_box_proc() { return fg24(ActiveTheme().box_proc); }

std::wstring ResolveThemesDir()
{
    wchar_t envBuf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"WINBTOP_THEMES", envBuf, MAX_PATH);
    if (n > 0 && n < MAX_PATH && PathFileExistsW(envBuf))
        return envBuf;

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring dir = std::wstring(exePath) + L"\\themes";
    if (PathFileExistsW(dir.c_str()))
        return dir;

    wchar_t cwd[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, cwd);
    std::wstring dir2 = std::wstring(cwd) + L"\\themes";
    if (PathFileExistsW(dir2.c_str()))
        return dir2;

    return L"themes";
}

static bool hexToRgb(const std::string &hexIn, Rgb &out)
{
    std::string hex = hexIn;
    if (!hex.empty() && hex[0] == '#')
        hex.erase(0, 1);
    if (hex.size() != 6)
        return false;
    auto h2 = [&](int i)
    { return std::stoi(hex.substr(i, 2), nullptr, 16); };
    try
    {
        out.r = std::clamp(h2(0), 0, 255);
        out.g = std::clamp(h2(2), 0, 255);
        out.b = std::clamp(h2(4), 0, 255);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static void set_if_default(Rgb &dst, const Rgb &def, const Rgb &val)
{
    if (dst.r == def.r && dst.g == def.g && dst.b == def.b)
        dst = val;
}

static void applyBtopKey(Theme &t, const std::string &key, const std::string &hex)
{
    Rgb c;
    if (!hexToRgb(hex, c))
        return;

    if (key == "main_bg")
        t.panel = c;
    else if (key == "main_fg")
        t.text = c;
    else if (key == "title")
        t.hdr = c;
    else if (key == "hi_fg")
        t.accent = c;

    else if (key == "selected_bg")
        t.sel_bg = c;
    else if (key == "selected_fg")
        t.sel_fg = c;

    else if (key == "inactive_fg")
        t.dim = c;
    else if (key == "graph_text")
        t.hdr = c;

    else if (key == "meter_bg")
        t.meter_bg = c;

    else if (key == "cpu_box")
        t.box_cpu = c;
    else if (key == "mem_box")
        t.box_mem = c;
    else if (key == "proc_box")
        t.box_proc = c;

    else if (key == "div_line")
    {
        t.frame = c;
        t.divider = c;
    }

    else if (key == "bar_lo" || key == "cpu_color_low")
        t.barLo = c;
    else if (key == "bar_hi" || key == "cpu_color_high")
        t.barHi = c;
}

static Theme loadOneTheme(const std::wstring &wpath)
{
    Theme tt;
    size_t p = wpath.find_last_of(L"\\/");
    size_t dot = wpath.find_last_of(L'.');
    if (p == std::wstring::npos)
        p = (size_t)-1;
    if (dot == std::wstring::npos || dot < p)
        dot = wpath.size();
    tt.name = wpath.substr(p + 1, dot - (p + 1));

    std::ifstream in(std::filesystem::path(wpath), std::ios::binary);
    if (!in)
        return tt;

    unsigned char bom[3] = {0, 0, 0};
    in.read(reinterpret_cast<char *>(bom), 3);
    if (!(bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF))
        in.seekg(0);

    std::regex re(R"(theme\[(.+?)\]\s*=\s*\"(#?[A-Fa-f0-9]{6})\")");
    std::smatch m;
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (std::regex_search(line, m, re) && m.size() >= 3)
            applyBtopKey(tt, m[1].str(), m[2].str());
    }

    auto lighten = [](int x)
    { return std::min(255, x + 14); };
    Rgb defOverlay = Theme{}.overlay;
    set_if_default(tt.overlay, defOverlay, {lighten(tt.panel.r), lighten(tt.panel.g), lighten(tt.panel.b)});

    Rgb defBg = Theme{}.bg;
    set_if_default(tt.bg, defBg, {lighten(tt.panel.r), lighten(tt.panel.g), lighten(tt.panel.b)});

    if (tt.frame.r == Theme{}.frame.r && tt.frame.g == Theme{}.frame.g && tt.frame.b == Theme{}.frame.b)
        tt.frame = tt.meter_bg;

    if (tt.divider.r == Theme{}.divider.r && tt.divider.g == Theme{}.divider.g && tt.divider.b == Theme{}.divider.b)
        tt.divider = tt.frame;

    auto relLum = [](const Rgb &c) -> double
    {
        auto chan = [](double u) -> double
        {
            u /= 255.0;
            return (u <= 0.03928) ? (u / 12.92) : std::pow((u + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * chan(c.r) + 0.7152 * chan(c.g) + 0.0722 * chan(c.b);
    };
    auto contrast = [&](const Rgb &a, const Rgb &b) -> double
    {
        double L1 = relLum(a), L2 = relLum(b);
        if (L1 < L2)
            std::swap(L1, L2);
        return (L1 + 0.05) / (L2 + 0.05);
    };
    auto pickReadable = [&](const Rgb &bg) -> Rgb
    {
        Rgb white{255, 255, 255}, black{0, 0, 0};
        return (contrast(white, bg) >= contrast(black, bg)) ? white : black;
    };
    auto ensureReadable = [&](Rgb &fg, const Rgb &bg, double minCr = 4.5)
    {
        if (contrast(fg, bg) < minCr)
            fg = pickReadable(bg);
    };

    ensureReadable(tt.text, tt.bg, 4.5);
    ensureReadable(tt.hdr, tt.bg, 4.5);

    if (contrast(tt.dim, tt.bg) < 3.0)
    {
        tt.dim = tt.text;
    }

    if (contrast(tt.sel_fg, tt.sel_bg) < 4.5)
    {
        tt.sel_fg = pickReadable(tt.sel_bg);
    }

    return tt;
}

bool ThemeManager::LoadDir(const std::wstring &dir)
{
    themes_.clear();

    WIN32_FIND_DATAW fd{};
    std::wstring mask = dir + L"\\*.theme";
    HANDLE h = FindFirstFileW(mask.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        Theme t = loadOneTheme(dir + L"\\" + fd.cFileName);
        themes_.push_back(std::move(t));
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    if (themes_.empty())
        return false;

    std::sort(themes_.begin(), themes_.end(), [](const Theme &a, const Theme &b)
              { return a.name < b.name; });
    idx_ = 0;
    SetActiveTheme(themes_[idx_]);
    return true;
}

const Theme &ThemeManager::Current() const { return ActiveTheme(); }

void ThemeManager::Next()
{
    if (themes_.empty())
        return;
    idx_ = (idx_ + 1) % themes_.size();
    SetActiveTheme(themes_[idx_]);
}

void ThemeManager::Prev()
{
    if (themes_.empty())
        return;
    idx_ = (idx_ + themes_.size() - 1) % themes_.size();
    SetActiveTheme(themes_[idx_]);
}

void ThemeManager::SetByName(const std::wstring &name)
{
    for (size_t i = 0; i < themes_.size(); ++i)
        if (themes_[i].name == name)
        {
            idx_ = i;
            SetActiveTheme(themes_[i]);
            return;
        }
}

std::vector<std::wstring> ThemeManager::Names() const
{
    std::vector<std::wstring> v;
    v.reserve(themes_.size());
    for (const auto &t : themes_)
        v.push_back(t.name);
    return v;
}
