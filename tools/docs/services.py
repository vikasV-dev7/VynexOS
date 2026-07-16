from .utils import write_preserved, ensure_dir
from .templates import SERVICE_TEMPLATE
import os

def generate_services(docs_dir: str, parsed_services: list):
    services_dir = os.path.join(docs_dir, "Services")
    ensure_dir(services_dir)
    
    for srv in parsed_services:
        interfaces = f"[[{srv['base_class']}]]" if srv['base_class'] else "None"
        
        content = SERVICE_TEMPLATE.format(
            name=srv['name'],
            purpose=f"Concrete implementation of {interfaces}.",
            source_path=srv['source_path'],
            interfaces=interfaces,
            dependencies="Constructor injected.",
            initialization="Via [[Composition Root]].",
            internal_design="To be documented.",
            interactions="To be documented.",
            future="To be documented."
        )
        
        filepath = os.path.join(services_dir, f"{srv['name']}.md")
        write_preserved(filepath, content)
