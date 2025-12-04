#pragma once
#include <iostream>
#include <format>

namespace Log {
    inline int Level = 1;

    namespace Color {
        constexpr const char* Reset  = "\033[0m";
        constexpr const char* Gray   = "\033[90m";
        constexpr const char* Cyan   = "\033[36m";
        constexpr const char* Green  = "\033[32m";
        constexpr const char* Yellow = "\033[33m";
        constexpr const char* Red    = "\033[31m";
    }

    template<typename... Args>
    void Trace(std::format_string<Args...> fmt, Args&&... args) {
        if (Level <= 0) std::cout << Color::Gray << "[trace] " << std::format(fmt, std::forward<Args>(args)...) << Color::Reset << "\n";
    }

    template<typename... Args>
    void Debug(std::format_string<Args...> fmt, Args&&... args) {
        if (Level <= 1) std::cout << Color::Cyan << "[debug] " << std::format(fmt, std::forward<Args>(args)...) << Color::Reset << "\n";
    }

    template<typename... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args) {
        if (Level <= 2) std::cout << Color::Green << "[info] " << std::format(fmt, std::forward<Args>(args)...) << Color::Reset << "\n";
    }

    template<typename... Args>
    void Warn(std::format_string<Args...> fmt, Args&&... args) {
        if (Level <= 3) std::cerr << Color::Yellow << "[warn] " << std::format(fmt, std::forward<Args>(args)...) << Color::Reset << "\n";
    }

    template<typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args) {
        if (Level <= 4) std::cerr << Color::Red << "[error] " << std::format(fmt, std::forward<Args>(args)...) << Color::Reset << "\n";
    }
}