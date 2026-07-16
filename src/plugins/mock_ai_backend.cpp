// This simulates a third-party AI backend plugin compiling against the C ABI.
#include "vynexos_abi/plugin.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

VynexPluginManifest vynex_plugin_get_manifest(void) {
    VynexPluginManifest manifest;
    manifest.plugin_name = "Mock_CPU_AI_Backend";
    manifest.description = "A dummy CPU-bound AI backend for VM testing.";
    manifest.version = {1, 0, 0};
    manifest.required_os_version = {0, 1, 0};
    return manifest;
}

bool vynex_plugin_initialize(void) {
    // In a real plugin, this would initialize llama.cpp or ONNX Runtime.
    return true;
}

void vynex_plugin_shutdown(void) {
    // Clean up resources.
}

void vynex_plugin_free_string(char* str) {
    if (str) free(str);
}

void vynex_plugin_free_buffer(uint8_t* buffer) {
    if (buffer) free(buffer);
}

#ifdef __cplusplus
}
#endif
