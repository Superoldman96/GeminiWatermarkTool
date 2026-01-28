/**
 * @file    path_formatter.hpp
 * @brief   Custom fmt formatter for std::filesystem::path with UTF-8 support
 * @author  AllenK (Kwyshell)
 * @date    2026.01.26
 * @license MIT
 *
 * @details
 * Solves Windows UTF-8 encoding issues when logging filesystem paths.
 *
 * Problem:
 *   - Windows: path.string() returns ANSI (local codepage, e.g., CP932, CP936, CP949, CP950)
 *   - spdlog/fmt expects UTF-8
 *
 * Solution:
 *   - Use path.u8string() which always returns UTF-8
 *   - C++20: u8string() returns std::u8string (char8_t) - needs reinterpret_cast
 *   - C++17: u8string() returns std::string (char) - works directly
 *
 * Usage:
 *   #include "path_formatter.hpp"
 *   spdlog::info("Processing: {}", some_path);  // Just works!
 */

#pragma once

#include <filesystem>
#include <string_view>
#include <fmt/format.h>

template <>
struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string_view> {
    auto format(const std::filesystem::path& p, format_context& ctx) const {
        auto u8 = p.u8string();
        std::string_view sv{
            reinterpret_cast<const char*>(u8.data()),
            u8.size()
        };
        return fmt::formatter<std::string_view>::format(sv, ctx);
    }
};
