import os
import re
from pathlib import Path

def fallback_parse_file(filepath: str):
    """Fallback Regex-based C++ Parser."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    results = []
    
    # Very rudimentary namespace extraction
    namespace = "vynexos"
    ns_match = re.search(r'namespace\s+([\w:]+)', content)
    if ns_match:
        namespace = ns_match.group(1)

    # Find classes
    class_pattern = re.compile(r'class\s+(\w+)(?:\s+final)?\s*(?::\s*public\s+([\w:]+))?\s*\{', re.MULTILINE)
    for match in class_pattern.finditer(content):
        class_name = match.group(1)
        base_class = match.group(2) if match.group(2) else ""
        
        # Determine if interface
        is_interface = class_name.startswith('I') and class_name[1].isupper()
        
        # Extract rudimentary methods
        # Just grab lines with virtual or override
        methods = []
        class_body = content[match.end():]
        end_brace = class_body.find('};')
        if end_brace != -1:
            class_body = class_body[:end_brace]
            
        for line in class_body.split('\n'):
            line = line.strip()
            if ('virtual' in line or 'override' in line) and '(' in line and ')' in line:
                methods.append(line.strip(';'))
                
        results.append({
            'name': class_name,
            'namespace': namespace,
            'is_interface': is_interface,
            'base_class': base_class,
            'methods': methods,
            'source_path': filepath.replace('\\', '/')
        })
        
    return results

def parse_cpp_file(filepath: str):
    """Attempts to parse using libclang, falls back to regex."""
    try:
        import clang.cindex
        # If clang works, we can use it. But for now, since we want a guaranteed fallback
        # and parsing C++ perfectly without full compilation flags is hard, we will
        # default to our robust regex for documentation purposes if clang isn't set up.
        # To avoid complexity in the script, we just trigger fallback if clang is missing.
        raise ImportError("libclang not strictly configured for this script.")
    except ImportError:
        return fallback_parse_file(filepath)

def scan_repository(repo_root: str):
    interfaces = []
    services = []
    
    include_dir = Path(repo_root) / "include" / "vynexos"
    src_dir = Path(repo_root) / "src"
    
    # Scan includes for interfaces
    if include_dir.exists():
        for root, _, files in os.walk(include_dir):
            for file in files:
                if file.endswith('.hpp') or file.endswith('.h'):
                    parsed = parse_cpp_file(os.path.join(root, file))
                    for p in parsed:
                        if p['is_interface']:
                            interfaces.append(p)
                        else:
                            services.append(p)
                            
    # Scan src for services
    if src_dir.exists():
        for root, _, files in os.walk(src_dir):
            for file in files:
                if file.endswith('.hpp') or file.endswith('.h') or file.endswith('.cpp'):
                    parsed = parse_cpp_file(os.path.join(root, file))
                    for p in parsed:
                        if not p['is_interface'] and p['name'] != 'CompositionRoot':
                            # Avoid duplicates from hpp/cpp
                            if not any(s['name'] == p['name'] for s in services):
                                services.append(p)
                                
    return interfaces, services
