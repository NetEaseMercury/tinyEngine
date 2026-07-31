#pragma once

#include <string>

/**
 * @file EnginePaths.hpp
 * @brief Runtime configuration of the engine resource root (res/).
 *
 * Historical convention: all asset paths (textures/shaders inside .ast files,
 * material-asset relative paths, etc.) are relative to the res/ directory.
 * In the standalone era res/ sat next to the exe and was located via the working
 * directory; in Editor mode the host (C# Editor) passes an absolute path at
 * te_init, and all path resolution inside the engine goes through this module.
 */
namespace te::paths {

/** @brief Internal storage; defaults to "res/" for the legacy working-directory mode. */
inline std::string& resRootStorage() {
    static std::string s = "res/";
    return s;
}

/** @brief Set the resource root (trailing separator added automatically); call once at the very start of engine init. */
inline void setResRoot(const std::string& absRoot) {
    std::string& s = resRootStorage();
    s = absRoot.empty() ? std::string("res/") : absRoot;
    if (!s.empty() && s.back() != '/' && s.back() != '\\') s += '/';
}

/** @brief Current resource root (relative "res/" or absolute; always ends with a separator). */
inline const std::string& resRoot() {
    return resRootStorage();
}

/** @brief Join a res-relative path into an openable path. */
inline std::string resolve(const std::string& rel) {
    if (rel.empty()) return {};
    return resRootStorage() + rel;
}

} // namespace te::paths
