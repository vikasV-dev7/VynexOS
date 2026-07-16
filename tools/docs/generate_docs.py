import os
import sys

# Add tools directory to sys.path so we can import modules
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')))

from tools.docs.parser import scan_repository
from tools.docs.adr import generate_adrs
from tools.docs.sprint import generate_sprints
from tools.docs.architecture import generate_architecture
from tools.docs.interfaces import generate_interfaces
from tools.docs.services import generate_services
from tools.docs.misc import generate_misc
from tools.docs.wiki import auto_link_text
from tools.docs.utils import ensure_dir

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
    docs_dir = os.path.join(repo_root, "docs")
    ensure_dir(docs_dir)

    print("Scanning repository...")
    interfaces, services = scan_repository(repo_root)

    print(f"Found {len(interfaces)} interfaces and {len(services)} services.")

    print("Generating static documentation...")
    generate_adrs(docs_dir)
    generate_sprints(docs_dir)
    generate_architecture(docs_dir)
    generate_misc(docs_dir)

    print("Generating API documentation...")
    generate_interfaces(docs_dir, interfaces)
    generate_services(docs_dir, services)

    # Wiki linking across all generated markdown files
    known_entities = [i['name'] for i in interfaces] + [s['name'] for s in services]
    known_entities += ["EventBus", "CompositionRoot", "WindowManager", "DesktopShell"]
    
    print("Applying Wiki links...")
    links_added = 0
    for root, _, files in os.walk(docs_dir):
        for file in files:
            if file.endswith('.md'):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                new_content = auto_link_text(content, known_entities)
                if new_content != content:
                    links_added += 1
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(new_content)

    print("\nDocumentation Generation Complete!")
    print(f"- Interfaces Discovered: {len(interfaces)}")
    print(f"- Services Discovered: {len(services)}")
    print(f"- Files Auto-linked: {links_added}")
    print(f"- Vault Location: {docs_dir}")

if __name__ == "__main__":
    main()
