from .utils import write_preserved, ensure_dir
from .templates import INTERFACE_TEMPLATE
import os

def generate_interfaces(docs_dir: str, parsed_interfaces: list):
    interfaces_dir = os.path.join(docs_dir, "Interfaces")
    ensure_dir(interfaces_dir)
    
    for iface in parsed_interfaces:
        methods = "\n".join([f"- `{m}`" for m in iface['methods']]) if iface['methods'] else "No public virtual methods found."
        
        content = INTERFACE_TEMPLATE.format(
            name=iface['name'],
            purpose=f"Interface for {iface['name']}.",
            namespace=iface['namespace'],
            source_path=iface['source_path'],
            responsibilities="To be documented.",
            dependencies="None (pure interface).",
            methods=methods,
            used_by="See [[Composition Root]]",
            implemented_by="See Services directory."
        )
        
        filepath = os.path.join(interfaces_dir, f"{iface['name']}.md")
        write_preserved(filepath, content)
