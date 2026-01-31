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
 *   - ImGui::Text expects UTF-8
 *
 * Solution:
 *   - Use path.u8string() which always returns UTF-8
 *   - C++20: u8string() returns std::u8string (char8_t) - needs reinterpret_cast
 *   - C++17: u8string() returns std::string (char) - works directly
 *
 * Usage:
 *   #include "path_formatter.hpp"
 *   spdlog::info("Processing: {}", some_path);  // Just works with fmt!
 *   ImGui::Text("File: %s", gwt::to_utf8(some_path).c_str());  // For ImGui
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <fmt/format.h>

namespace gwt {

/**
 * Convert filesystem path to UTF-8 encoded std::string
 * 
 * This handles the C++20 char8_t issue where u8string() returns std::u8string
 * instead of std::string.
 *
 * @param path  The filesystem path to convert
 * @return      UTF-8 encoded string
 */
inline std::string to_utf8(const std::filesystem::path& path) {
    auto u8str = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(u8str.data()),
        u8str.size()
    );
}

/**
 * Convert path filename to UTF-8 encoded std::string
 * Convenience function for common use case
 */
inline std::string filename_utf8(const std::filesystem::path& path) {
    return to_utf8(path.filename());
}

}  // namespace gwt

// =============================================================================
// fmt formatter specialization for std::filesystem::path
// =============================================================================

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
