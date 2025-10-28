#include "util.h"

#include <sstream>
#include <iomanip>
#include <io.h>
#include <fcntl.h>
#include <cwchar>
#include <cstring> 

std::wstring RST() { return L"\x1b[0m"; }

std::wstring fg24(int r, int g, int b)
{
    wchar_t buf[64];
    swprintf(buf, 64, L"\x1b[38;2;%d;%d;%dm", r, g, b);
    return buf;
}

std::wstring bg24(int r, int g, int b)
{
    wchar_t buf[64];
    swprintf(buf, 64, L"\x1b[48;2;%d;%d;%dm", r, g, b);
    return buf;
}

bool InitConsole()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD outMode = 0;
    if (!GetConsoleMode(hOut, &outMode))
        return false;
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
    SetConsoleMode(hOut, outMode);

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD inMode = 0;
    GetConsoleMode(hIn, &inMode);

    inMode |= ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
    inMode &= ~ENABLE_QUICK_EDIT_MODE;
    inMode &= ~ENABLE_INSERT_MODE;
    SetConsoleMode(hIn, inMode);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    DWORD w;
    const char *kEnterAlt = "\x1b[?1049h\x1b[?25l"; // alt screen + hide cursor
    WriteConsoleA(hOut, kEnterAlt, (DWORD)std::strlen(kEnterAlt), &w, nullptr);
    return true;
}

void ShutdownConsole()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD w;
    const char *kLeaveAlt = "\x1b[?25h\x1b[?1049l"; // show cursor + leave alt screen
    WriteConsoleA(hOut, kLeaveAlt, (DWORD)std::strlen(kLeaveAlt), &w, nullptr);
}

void ClearScreen()
{
    DWORD written;
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteConsoleA(hout, "\x1b[2J\x1b[H", 7, &written, nullptr);
}

void MoveCursor(short row, short col)
{
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (int)row, (int)col);
    DWORD w;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), buf, n, &w, nullptr);
}

void SetTitle(std::wstring_view title)
{
    SetConsoleTitleW(std::wstring(title).c_str());
}

COORD GetConsoleSize()
{
    CONSOLE_SCREEN_BUFFER_INFO info{};
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    short cols = info.srWindow.Right - info.srWindow.Left + 1;
    short rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    return {cols, rows};
}

std::wstring FormatBytesULONGLONG(ULONGLONG bytes)
{
    static const wchar_t *units[] = {L"B", L"KiB", L"MiB", L"GiB", L"TiB"};
    int u = 0;
    long double val = (long double)bytes;
    while (val >= 1024.0 && u < 4)
    {
        val /= 1024.0;
        ++u;
    }
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(val >= 100 ? 0 : (val >= 10 ? 1 : 2))
       << val << L" " << units[u];
    return ss.str();
}

std::wstring PadRight(std::wstring s, size_t width)
{
    if (s.size() >= width)
        return s.substr(0, width);
    s.append(width - s.size(), L' ');
    return s;
}
