#include "vynexos/core/loaded_plugin.hpp"

namespace vynexos::core {

LoadedPlugin::LoadedPlugin(std::shared_ptr<hal::ISharedLibrary> library,
                           VynexPluginManifest manifest,
                           std::unique_ptr<PluginContext> context,
                           VynexPluginContext c_context)
    : m_library(std::move(library)),
      m_manifest(manifest),
      m_cxx_context(std::move(context)),
      m_c_context(c_context) {
    load_symbols();
}

LoadedPlugin::~LoadedPlugin() {
    // Ensure safe teardown if destroyed prematurely
    if (m_state == PluginState::Running || m_state == PluginState::Started) {
        transition_stop();
    }
    if (m_state == PluginState::Stopped || m_state == PluginState::ServicesRegistered || m_state == PluginState::Initialized) {
        transition_shutdown();
    }
    if (m_state == PluginState::Shutdown || m_state == PluginState::Created) {
        transition_destroy();
    }
}

std::string_view LoadedPlugin::get_name() const {
    return m_manifest.plugin_name;
}

std::string_view LoadedPlugin::get_description() const {
    return m_manifest.description;
}

void LoadedPlugin::load_symbols() {
    fn_create = reinterpret_cast<CreateFunc>(m_library->get_symbol("vynex_plugin_create"));
    fn_init = reinterpret_cast<InitFunc>(m_library->get_symbol("vynex_plugin_initialize"));
    fn_reg = reinterpret_cast<RegServicesFunc>(m_library->get_symbol("vynex_plugin_register_services"));
    fn_start = reinterpret_cast<StartFunc>(m_library->get_symbol("vynex_plugin_start"));
    fn_stop = reinterpret_cast<StopFunc>(m_library->get_symbol("vynex_plugin_stop"));
    fn_shutdown = reinterpret_cast<ShutdownFunc>(m_library->get_symbol("vynex_plugin_shutdown"));
    fn_destroy = reinterpret_cast<DestroyFunc>(m_library->get_symbol("vynex_plugin_destroy"));
}

bool LoadedPlugin::has_valid_abi() const {
    return fn_create && fn_init && fn_reg && fn_start && fn_stop && fn_shutdown && fn_destroy;
}

bool LoadedPlugin::transition_create() {
    if (m_state != PluginState::Loaded) return false;
    if (fn_create && !fn_create()) return false;
    m_state = PluginState::Created;
    return true;
}

bool LoadedPlugin::transition_initialize() {
    if (m_state != PluginState::Created) return false;
    if (fn_init && !fn_init(&m_c_context)) return false;
    m_state = PluginState::Initialized;
    return true;
}

bool LoadedPlugin::transition_register_services() {
    if (m_state != PluginState::Initialized) return false;
    if (fn_reg && !fn_reg()) return false;
    m_state = PluginState::ServicesRegistered;
    return true;
}

bool LoadedPlugin::transition_start() {
    if (m_state != PluginState::ServicesRegistered) return false;
    if (fn_start && !fn_start()) return false;
    m_state = PluginState::Started;
    return true;
}

void LoadedPlugin::transition_to_running() {
    if (m_state == PluginState::Started) {
        m_state = PluginState::Running;
    }
}

void LoadedPlugin::transition_stop() {
    if (m_state != PluginState::Running && m_state != PluginState::Started) return;
    if (fn_stop) fn_stop();
    m_state = PluginState::Stopped;
}

void LoadedPlugin::transition_shutdown() {
    if (m_state != PluginState::Stopped && m_state != PluginState::ServicesRegistered && m_state != PluginState::Initialized) return;
    if (fn_shutdown) fn_shutdown();
    m_state = PluginState::Shutdown;
}

void LoadedPlugin::transition_destroy() {
    if (m_state != PluginState::Shutdown && m_state != PluginState::Created) return;
    if (fn_destroy) fn_destroy();
    m_state = PluginState::Destroyed;
}

} // namespace vynexos::core
