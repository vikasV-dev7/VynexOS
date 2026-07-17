#include "vynexos_abi/plugin.h"
#include <stddef.h>

VynexPluginManifest vynex_plugin_get_manifest(void) {
    VynexPluginManifest m;
    m.plugin_name = "V2TestPlugin";
    m.description = "Requires OS v2";
    m.version.major = 1; m.version.minor = 0; m.version.patch = 0;
    m.required_os_version.major = 2; m.required_os_version.minor = 0; m.required_os_version.patch = 0;
    return m;
}

bool vynex_plugin_create(void) { return true; }
bool vynex_plugin_initialize(const VynexPluginContext* ctx) { (void)ctx; return true; }
bool vynex_plugin_register_services(void) { return true; }
bool vynex_plugin_start(void) { return true; }
void vynex_plugin_stop(void) {}
void vynex_plugin_shutdown(void) {}
void vynex_plugin_destroy(void) {}
void vynex_plugin_free_string(char* str) { (void)str; }
void vynex_plugin_free_buffer(uint8_t* buffer) { (void)buffer; }
