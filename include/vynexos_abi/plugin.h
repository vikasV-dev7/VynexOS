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

// ============================================================================
// Plugin Context (Dependency Injection)
// ============================================================================
typedef void (*VynexLogFunc)(int level, const char* message, void* user_data);
typedef bool (*VynexPublishEventFunc)(const char* topic, const uint8_t* payload, size_t size, void* user_data);

typedef void (*VynexTaskCallback)(void* task_data);
typedef bool (*VynexScheduleTaskFunc)(VynexTaskCallback task_func, void* task_data, void* user_data);

typedef const char* (*VynexGetConfigFunc)(const char* key, void* user_data);

typedef struct {
    void* internal_context;
    VynexLogFunc log;
    VynexPublishEventFunc publish_event;
    VynexScheduleTaskFunc schedule_task;
    VynexGetConfigFunc get_configuration;
} VynexPluginContext;

// ============================================================================
// Plugin Lifecycle Operations
// ============================================================================
VynexPluginManifest vynex_plugin_get_manifest(void);

bool vynex_plugin_create(void);
bool vynex_plugin_initialize(const VynexPluginContext* ctx);
bool vynex_plugin_register_services(void);
bool vynex_plugin_start(void);

void vynex_plugin_stop(void);
void vynex_plugin_shutdown(void);
void vynex_plugin_destroy(void);

// Memory lifecycle (called by OS when it is done with plugin-allocated memory)
void vynex_plugin_free_string(char* str);
void vynex_plugin_free_buffer(uint8_t* buffer);

#ifdef __cplusplus
}
#endif
