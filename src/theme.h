#pragma once
#include <string>
#include <vector>

struct Rgb
{
    int r = 255, g = 255, b = 255;
};

struct Theme
{
    std::wstring name = L"Default";

    Rgb text{220, 220, 220};
    Rgb dim{160, 160, 160};
    Rgb hdr{200, 200, 200};

    Rgb panel{20, 20, 20};
    Rgb bg{28, 28, 28};
    Rgb overlay{12, 12, 12};

    Rgb accent{120, 220, 255};
    Rgb sel_fg{255, 255, 255};
    Rgb sel_bg{120, 60, 140};

    Rgb meter_bg{60, 60, 60};
    Rgb frame{90, 90, 90};
    Rgb divider{60, 60, 60};

    Rgb barLo{120, 255, 60};
    Rgb barHi{255, 120, 60};

    Rgb box_cpu{120, 200, 255};
    Rgb box_mem{180, 255, 120};
    Rgb box_proc{180, 160, 255};
};

class ThemeManager
{
public:
    bool LoadDir(const std::wstring &dir);
    const Theme &Current() const;
    void Next();
    void Prev();
    void SetByName(const std::wstring &name);
    std::vector<std::wstring> Names() const;

private:
    std::vector<Theme> themes_;
    size_t idx_ = 0;
};

std::wstring RST();
std::wstring fg24(int r, int g, int b);
std::wstring bg24(int r, int g, int b);
inline std::wstring fg24(const Rgb &c) { return fg24(c.r, c.g, c.b); }
inline std::wstring bg24(const Rgb &c) { return bg24(c.r, c.g, c.b); }

const Theme &ActiveTheme();
void SetActiveTheme(const Theme &t);

std::wstring col_ok();
std::wstring col_warn();
std::wstring col_crit();
std::wstring col_dim();
std::wstring col_hdr();
std::wstring col_text();
std::wstring col_accent();

std::wstring col_box_cpu();
std::wstring col_box_mem();
std::wstring col_box_proc();

std::wstring ResolveThemesDir();
