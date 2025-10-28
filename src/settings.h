#pragma once
#include <string>

struct Settings
{
    std::wstring themeName;
    int hz = 5;
};

bool LoadSettings(Settings &s);
bool SaveSettings(const Settings &s);
