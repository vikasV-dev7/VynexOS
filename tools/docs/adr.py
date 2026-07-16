from .utils import write_preserved
import os

ADRS = {
    "ADR-000 Index": "# ADR Index\n\n1. [[ADR-001 Runtime Architecture]]\n2. [[ADR-002 Dependency Injection]]\n3. [[ADR-003 Event Bus]]\n4. [[ADR-004 Window Manager]]\n5. [[ADR-005 Notification Pipeline]]\n6. [[ADR-006 Plugin Manager]]\n7. [[ADR-007 HAL]]\n",
    "ADR-001 Runtime Architecture": "## Problem\nNeed a modular OS runtime.\n\n## Chosen Solution\nHosted C++23 Runtime simulating hardware.\n",
    "ADR-002 Dependency Injection": "## Problem\nGlobal state creates tight coupling.\n\n## Chosen Solution\nStrict Constructor Dependency Injection via Composition Root.\n",
    "ADR-003 Event Bus": "## Problem\nSubsystems need to communicate without tight coupling.\n\n## Chosen Solution\nIn-Memory Event Bus using `std::any` payloads.\n",
    "ADR-004 Window Manager": "## Problem\nWindow management vs rendering.\n\n## Chosen Solution\nWindowManager is pure state; Compositor handles rendering.\n",
    "ADR-005 Notification Pipeline": "## Problem\nHow to push notifications to DesktopShell without coupling.\n\n## Chosen Solution\nPublish over EventBus; DesktopShell subscribes.\n",
    "ADR-006 Plugin Manager": "## Problem\nDynamic loading.\n\n## Chosen Solution\nPlatform-independent IPluginManager, internal dynamic loaders.\n",
    "ADR-007 HAL": "## Problem\nHardware dependence.\n\n## Chosen Solution\nStrict HAL interfaces (Audio, Compute, Input, Display).\n"
}

def generate_adrs(docs_dir: str):
    adr_dir = os.path.join(docs_dir, "Decisions")
    from .utils import ensure_dir
    ensure_dir(adr_dir)
    
    for name, content in ADRS.items():
        filepath = os.path.join(adr_dir, f"{name}.md")
        write_preserved(filepath, f"# {name}\n\n{content}")
