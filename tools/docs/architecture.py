from .utils import write_preserved, ensure_dir
from .diagrams import generate_system_architecture, generate_composition_root, generate_event_bus_flow
import os

PAGES = {
    "System Overview": f"## Architecture Overview\n{generate_system_architecture()}\n\n## Subsystems\n- [[Runtime]]\n- [[HAL]]\n- [[Desktop Shell]]",
    "Composition Root": f"## Composition Root\n{generate_composition_root()}",
    "Event Bus": f"## Event Bus\n{generate_event_bus_flow()}",
    "Runtime": "## Runtime\nProvides core facilities.",
    "Bootstrap": "## Bootstrap\nEntry point for `vynex_init`.",
    "Dependency Injection": "## Dependency Injection\nStrict constructor injection.",
    "IPC": "## IPC\nLocal IPC Framework.",
    "Plugin Manager": "## Plugin Manager\nDynamic loading abstraction.",
    "Logger": "## Logger\nSystem logging.",
    "HAL": "## HAL\nHardware Abstraction Layer.",
    "Desktop Shell": "## Desktop Shell\nMain UI application.",
    "Window Manager": "## Window Manager\nState management for windows.",
    "Compositor": "## Compositor\nRendering pipeline.",
    "Notification System": "## Notification System\nEventBus driven notifications.",
    "Terminal": "## Terminal\nBasic CLI.",
    "File Explorer": "## File Explorer\nBasic VFS navigation.",
    "Runtime Loop": "## Runtime Loop\nEvent loop execution.",
    "Startup Sequence": "## Startup Sequence\nInitialization flow."
}

def generate_architecture(docs_dir: str):
    arch_dir = os.path.join(docs_dir, "Architecture")
    ensure_dir(arch_dir)
    for name, content in PAGES.items():
        filepath = os.path.join(arch_dir, f"{name}.md")
        write_preserved(filepath, f"# {name}\n\n{content}")
