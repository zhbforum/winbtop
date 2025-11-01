#include "settings.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <codecvt>
#include <locale>

static std::filesystem::path SettingsPath()
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::filesystem::path p(exe);
    return p.parent_path() / L"winbtop.ini";
}

static std::wstring Trim(const std::wstring &s)
{
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos)
        return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::wstring FromUtf8(const std::string &u8)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> cvt;
    return cvt.from_bytes(u8);
}
static std::string ToUtf8(const std::wstring &ws)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> cvt;
    return cvt.to_bytes(ws);
}

bool LoadSettings(Settings &s)
{
    auto path = SettingsPath();
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;

    std::string file;
    in.seekg(0, std::ios::end);
    file.resize((size_t)in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&file[0], (std::streamsize)file.size());

    size_t pos = 0;
    while (pos < file.size())
    {
        size_t eol = file.find('\n', pos);
        if (eol == std::string::npos)
            eol = file.size();
        std::string line8 = file.substr(pos, eol - pos);
        if (!line8.empty() && line8.back() == '\r')
            line8.pop_back();

        std::wstring line = FromUtf8(line8);
        auto eq = line.find(L'=');
        if (eq != std::wstring::npos)
        {
            std::wstring k = Trim(line.substr(0, eq));
            std::wstring v = Trim(line.substr(eq + 1));
            if (k == L"theme")
                s.themeName = v;
            else if (k == L"hz")
            {
                try
                {
                    s.hz = std::max(1, std::stoi(v));
                }
                catch (...)
                {
                }
            }
        }
        pos = eol + 1;
    }
    return true;
}

bool SaveSettings(const Settings &s)
{
    auto path = SettingsPath();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;

    std::string line1 = "theme=" + ToUtf8(s.themeName) + "\n";
    std::string line2 = "hz=" + std::to_string(s.hz) + "\n";
    out.write(line1.data(), (std::streamsize)line1.size());
    out.write(line2.data(), (std::streamsize)line2.size());
    return true;
}
