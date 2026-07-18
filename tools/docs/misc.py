from .utils import write_preserved, ensure_dir
import os

def generate_misc(docs_dir: str):
    # Root level files (MANUALLY MAINTAINED NOW)
    # write_preserved(os.path.join(docs_dir, "Home.md"), "# VynexOS Documentation Home\n\n## Quick Links\n- [[System Overview]]\n- [[Roadmap]]\n- [[Changelog]]\n- [[Development Diary]]\n- [[Glossary]]")
    # write_preserved(os.path.join(docs_dir, "Roadmap.md"), "# Roadmap\n\n## Future Milestones\n- [[Kernel]]\n- [[Memory Manager]]\n- [[Scheduler]]\n- [[Virtual File System]]\n- [[Drivers]]\n- [[Networking]]\n- [[AI Runtime]]")
    # write_preserved(os.path.join(docs_dir, "Changelog.md"), "# Changelog\n\n- **v1.0**: Runtime Architecture 1.0 Complete.")
    # write_preserved(os.path.join(docs_dir, "Development Diary.md"), "# Development Diary\n\nNotes and reflections.")
    # write_preserved(os.path.join(docs_dir, "Glossary.md"), "# Glossary\n\n- **HAL**: Hardware Abstraction Layer.")

    # Build
    build_dir = os.path.join(docs_dir, "Build")
    ensure_dir(build_dir)
    for p in ["Build Instructions", "Visual Studio", "CMake", "Linux", "WSL", "QEMU"]:
        write_preserved(os.path.join(build_dir, f"{p}.md"), f"# {p}\n\nBuild documentation.")

    # Testing
    test_dir = os.path.join(docs_dir, "Testing")
    ensure_dir(test_dir)
    for p in ["Build Verification", "Runtime Verification"]:
        write_preserved(os.path.join(test_dir, f"{p}.md"), f"# {p}\n\nTesting workflows.")

    # Developer Guide
    dev_dir = os.path.join(docs_dir, "Developer Guide")
    ensure_dir(dev_dir)
    for p in ["Coding Standards", "Folder Structure", "Naming Convention", "Dependency Rules", "Architecture Rules", "Review Checklist", "How to Add a Service", "How to Add a Driver", "How to Add an Application", "Testing Workflow"]:
        write_preserved(os.path.join(dev_dir, f"{p}.md"), f"# {p}\n\nDeveloper guide documentation.")

    # Future
    future_dir = os.path.join(docs_dir, "Future")
    ensure_dir(future_dir)
    for p in ["Kernel", "Memory Manager", "Scheduler", "Process Manager", "Virtual File System", "Drivers", "Networking", "AI Runtime"]:
        write_preserved(os.path.join(future_dir, f"{p}.md"), f"# {p}\n\nFuture roadmap item.")
