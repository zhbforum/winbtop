#pragma once
#include <string>
#include <string_view>
#include <windows.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef ENABLE_QUICK_EDIT_MODE
#define ENABLE_QUICK_EDIT_MODE 0x0040
#endif

std::wstring RST();
std::wstring fg24(int r, int g, int b);
std::wstring bg24(int r, int g, int b);

bool InitConsole();
void ShutdownConsole();
void ClearScreen();
void MoveCursor(short row, short col);
void SetTitle(std::wstring_view title);
COORD GetConsoleSize();

std::wstring FormatBytesULONGLONG(ULONGLONG bytes);
std::wstring PadRight(std::wstring s, size_t width);
