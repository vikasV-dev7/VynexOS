from .utils import write_preserved
import os

def generate_sprints(docs_dir: str):
    sprints_dir = os.path.join(docs_dir, "Sprints")
    from .utils import ensure_dir
    ensure_dir(sprints_dir)
    
    # Sprints 1 to 12
    for i in range(1, 13):
        num = f"{i:02d}"
        content = f"# Sprint {num}\n\n> [!WARNING] Historical implementation details unavailable.\n\n## Inferred Architectural Evolution\nThis sprint contributed to the foundations of the V1.0 architecture.\n"
        write_preserved(os.path.join(sprints_dir, f"Sprint {num}.md"), content)
        
    # Sprint 13
    sprint_13 = """# Sprint 13
## Objective
Complete Architecture Version 1.0.

## Architecture Changes
- Implemented `BasicPluginManager`
- Implemented `MockAudioDriver` and `MockComputeDriver`
- Integrated `BasicNotificationService` via `IEventBus`
- Wired via `CompositionRoot`

## Verification
Clean `/WX` MSVC C++23 build.
"""
    write_preserved(os.path.join(sprints_dir, "Sprint 13.md"), sprint_13)
