import os
import re
from pathlib import Path
from typing import List, Tuple
from .model import Interface, Service, Method, Metadata, IR_VERSION
from .plugins import IDocumentationParser

class RegexParser(IDocumentationParser):
    @property
    def name(self) -> str: return "RegexParser"
    @property
    def version(self) -> str: return "1.1.0"
    @property
    def supported_ir_versions(self) -> list[str]: return [IR_VERSION]

    def parse_file(self, filepath: str) -> List[dict]:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        results = []
        namespace = "vynexos"
        ns_match = re.search(r'namespace\s+([\w:]+)', content)
        if ns_match:
            namespace = ns_match.group(1)

        class_pattern = re.compile(r'class\s+(\w+)(?:\s+final)?\s*(?::\s*public\s+([\w:]+))?\s*\{', re.MULTILINE)
        for match in class_pattern.finditer(content):
            class_name = match.group(1)
            base_class = match.group(2) if match.group(2) else ""
            is_interface = class_name.startswith('I') and class_name[1].isupper()

            # Attempt to extract rudimentary block comment right before class
            before_class = content[:match.start()]
            last_comment_idx = before_class.rfind('/*')
            metadata = Metadata()
            if last_comment_idx != -1 and before_class.find('*/', last_comment_idx) != -1:
                comment_block = before_class[last_comment_idx:before_class.find('*/', last_comment_idx)+2]
                metadata.raw_doxygen = comment_block
                # Simple extraction
                for line in comment_block.split('\n'):
                    if '@Purpose' in line: metadata.purpose = line.split('@Purpose')[-1].strip()
                    if '@ThreadSafety' in line: metadata.thread_safety = line.split('@ThreadSafety')[-1].strip()

            methods = []
            class_body = content[match.end():]
            end_brace = class_body.find('};')
            if end_brace != -1:
                class_body = class_body[:end_brace]
                
            for line in class_body.split('\n'):
                line = line.strip()
                if ('virtual' in line or 'override' in line) and '(' in line and ')' in line:
                    methods.append(Method(name=line.split('(')[0].split()[-1], signature=line, return_type="", is_virtual=True, is_noexcept=False, is_nodiscard=False))
                    
            results.append({
                'name': class_name,
                'namespace': namespace,
                'is_interface': is_interface,
                'base_classes': [base_class] if base_class else [],
                'methods': methods,
                'metadata': metadata,
                'source_path': filepath.replace('\\', '/')
            })
            
        return results

    def scan_repository(self, repo_root: str) -> Tuple[List[Interface], List[Service]]:
        interfaces, services = [], []
        include_dir = Path(repo_root) / "include" / "vynexos"
        src_dir = Path(repo_root) / "src"
        
        for d in [include_dir, src_dir]:
            if d.exists():
                for root, _, files in os.walk(d):
                    for file in files:
                        if file.endswith(('.hpp', '.h', '.cpp')):
                            parsed = self.parse_file(os.path.join(root, file))
                            for p in parsed:
                                entity = Interface(**p) if p['is_interface'] else Service(**p)
                                if entity.is_interface:
                                    interfaces.append(entity)
                                else:
                                    if entity.name != 'CompositionRoot' and not any(s.name == entity.name for s in services):
                                        services.append(entity)
        return interfaces, services
