def create_diagram(title: str, graph_type: str, nodes: list, edges: list) -> str:
    """
    Creates a Mermaid diagram block.
    """
    lines = [f"```mermaid", f"{graph_type} title {title}"]
    for node in nodes:
        lines.append(f"    {node}")
    for edge in edges:
        lines.append(f"    {edge}")
    lines.append("```")
    return "\n".join(lines)

def generate_system_architecture() -> str:
    return create_diagram("System Architecture", "graph TD",
        ["Core[vynex_core]", "HAL[vynex_hal]", "Desktop[vynex_desktop]", "Apps[vynex_apps]", "Init[vynex_init]"],
        ["Init --> Apps", "Apps --> Desktop", "Desktop --> Core", "HAL --> Core", "Init --> HAL", "Init --> Desktop", "Init --> Core"]
    )

def generate_composition_root() -> str:
    return create_diagram("Composition Root Dependency Graph", "graph LR",
        ["CR[Composition Root]", "EventBus", "WindowManager", "DesktopShell", "AudioDriver"],
        ["CR --> EventBus", "CR --> WindowManager", "CR --> DesktopShell", "CR --> AudioDriver"]
    )

def generate_event_bus_flow() -> str:
    return create_diagram("EventBus Communication Flow", "sequenceDiagram",
        ["participant P as Publisher", "participant EB as EventBus", "participant S as Subscriber"],
        ["P->>EB: publish(Event)", "EB->>S: trigger callback(Event)"]
    )
