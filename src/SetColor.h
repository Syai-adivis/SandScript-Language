#ifndef CONSOLE_COLOR_H
#define CONSOLE_COLOR_H
#include <iostream>

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
