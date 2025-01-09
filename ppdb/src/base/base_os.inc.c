/*
 * base_os.inc.c - Operating System Interface Implementation
 */

#include "internal/base.h"
#include "cosmopolitan.h"

static ppdb_os_type_t g_os_type = PPDB_OS_UNKNOWN;

ppdb_os_type_t ppdb_base_get_os_type(void) {
    if (g_os_type != PPDB_OS_UNKNOWN) {
        return g_os_type;
    }

    // Use Cosmopolitan's built-in OS detection
    if (IsWindows()) {
        g_os_type = PPDB_OS_WINDOWS;
    } else if (IsLinux()) {
        g_os_type = PPDB_OS_LINUX;
    } else if (IsMachos()) {
        g_os_type = PPDB_OS_MACOS;
    } else if (IsFreebsd() || IsOpenbsd() || IsNetbsd()) {
        g_os_type = PPDB_OS_BSD;
    }

    return g_os_type;
}

const char* ppdb_base_get_os_name(void) {
    switch (ppdb_base_get_os_type()) {
        case PPDB_OS_WINDOWS: return "Windows";
        case PPDB_OS_LINUX:   return "Linux";
        case PPDB_OS_MACOS:   return "macOS";
        case PPDB_OS_BSD:     return "BSD";
        default:              return "Unknown";
    }
}

bool ppdb_base_is_windows(void) {
    return ppdb_base_get_os_type() == PPDB_OS_WINDOWS;
}

bool ppdb_base_is_unix(void) {
    ppdb_os_type_t os = ppdb_base_get_os_type();
    return os == PPDB_OS_LINUX || os == PPDB_OS_MACOS || os == PPDB_OS_BSD;
} 