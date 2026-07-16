#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// VynexOS Stable Plugin ABI - Version 1.0
// ============================================================================
// Memory Ownership Rules:
// 1. Any string or buffer returned by a plugin must be freed by calling 
//    the plugin's designated free function.
// 2. The host OS will never call free() or delete on memory allocated by the plugin.
// ============================================================================

typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} VynexVersion;

typedef struct {
    const char* plugin_name;
    const char* description;
    VynexVersion version;
    VynexVersion required_os_version;
} VynexPluginManifest;

// Base operations all plugins must export
VynexPluginManifest vynex_plugin_get_manifest(void);
bool vynex_plugin_initialize(void);
void vynex_plugin_shutdown(void);

// Memory lifecycle (called by OS when it is done with plugin-allocated memory)
void vynex_plugin_free_string(char* str);
void vynex_plugin_free_buffer(uint8_t* buffer);

#ifdef __cplusplus
}
#endif
