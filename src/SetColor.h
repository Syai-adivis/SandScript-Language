#ifndef CONSOLE_COLOR_H
#define CONSOLE_COLOR_H
#include <iostream>

#ifdef _WIN32
#include <windows.h>
static bool g_consoleAnsiEnabled = false;

static void enableWindowsAnsi()
{
    if (g_consoleAnsiEnabled)
        return;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
        return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode))
    {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
    g_consoleAnsiEnabled = true;
}

static void setWinColor(WORD attr)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hOut, attr);
}
#else
static inline void enableWindowsAnsi() {}
#endif

namespace Console
{
    enum class Color
    {
        Reset,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,
        BrightRed,
        BrightGreen
    };

    inline void setColor(Color c)
    {
#ifdef _WIN32
        enableWindowsAnsi();
#endif
        switch (c)
        {
        case Color::Reset:
            std::cout << "\033[0m";
            break;
        case Color::Red:
            std::cout << "\033[31m";
            break;
        case Color::Green:
            std::cout << "\033[32m";
            break;
        case Color::Yellow:
            std::cout << "\033[33m";
            break;
        case Color::Blue:
            std::cout << "\033[34m";
            break;
        case Color::Magenta:
            std::cout << "\033[35m";
            break;
        case Color::Cyan:
            std::cout << "\033[36m";
            break;
        case Color::White:
            std::cout << "\033[37m";
            break;
        case Color::BrightRed:
            std::cout << "\033[91m";
            break;
        case Color::BrightGreen:
            std::cout << "\033[92m";
            break;
        }
    }

    // 带颜色打印一行，自动重置
    template <typename T>
    void printColored(const T &text, Color color)
    {
        setColor(color);
        std::cout << text;
        setColor(Color::Reset);
    }

    template <typename T>
    void printlnColored(const T &text, Color color)
    {
        setColor(color);
        std::cout << text << '\n';
        setColor(Color::Reset);
    }
}
#endif
