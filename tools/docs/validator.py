import os
import re
from typing import List, Dict

class ValidationReport:
    def __init__(self):
        self.broken_wiki_links = []
        self.broken_md_links = []
        self.orphaned_pages = []
        self.duplicate_titles = []
        self.missing_metadata = []
        self.invalid_mermaid = []
    
    def has_errors(self) -> bool:
        return any([
            self.broken_wiki_links, self.broken_md_links, self.orphaned_pages,
            self.duplicate_titles, self.missing_metadata, self.invalid_mermaid
        ])
    
    def print_report(self):
        print("=== Documentation Validation Report ===")
        if not self.has_errors():
            print("✓ All validation checks passed.")
            return

        if self.broken_wiki_links:
            print(f"\n[!] Broken Wiki Links ({len(self.broken_wiki_links)}):")
            for f, l in self.broken_wiki_links: print(f"  - {f}: [[{l}]]")
            
        if self.broken_md_links:
            print(f"\n[!] Broken Markdown Links ({len(self.broken_md_links)}):")
            for f, l in self.broken_md_links: print(f"  - {f}: [{l}]()")
            
        if self.orphaned_pages:
            print(f"\n[!] Orphaned Pages ({len(self.orphaned_pages)}):")
            for p in self.orphaned_pages: print(f"  - {p}")
            
        if self.duplicate_titles:
            print(f"\n[!] Duplicate Titles ({len(self.duplicate_titles)}):")
            for t, files in self.duplicate_titles: print(f"  - '{t}' found in: {', '.join(files)}")
            
        if self.invalid_mermaid:
            print(f"\n[!] Invalid Mermaid Diagrams ({len(self.invalid_mermaid)}):")
            for f, reason in self.invalid_mermaid: print(f"  - {f}: {reason}")
            
        if self.missing_metadata:
            print(f"\n[!] Missing Metadata ({len(self.missing_metadata)}):")
            for f in self.missing_metadata: print(f"  - {f}")
        print("=======================================")

from .plugins import IDocumentationValidator
from .model import IR_VERSION

class DocumentationValidator(IDocumentationValidator):
    @property
    def name(self) -> str: return "DocumentationValidator"
    @property
    def version(self) -> str: return "1.1.0"
    @property
    def supported_ir_versions(self) -> list[str]: return [IR_VERSION]

    def __init__(self):
        pass
        
    def validate(self, docs_dir: str) -> bool:
        self.report = ValidationReport()
        all_pages = set()
        referenced_pages = set(['Home']) # Home is always the root
        
        page_titles = {}
        
        for root, _, files in os.walk(docs_dir):
            if '.obsidian' in root: continue
            for file in files:
                if file.endswith('.md'):
                    page_name = file[:-3]
                    all_pages.add(page_name)
                    
                    filepath = os.path.join(root, file)
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read()
                        
                    # Check titles (first H1)
                    title_match = re.search(r'^#\s+(.+)$', content, re.MULTILINE)
                    if title_match:
                        title = title_match.group(1).strip()
                        if title not in page_titles:
                            page_titles[title] = []
                        page_titles[title].append(file)
                        
                    # Check Wiki Links
                    for match in re.finditer(r'\[\[(.*?)\]\]', content):
                        target = match.group(1).split('|')[0].strip()
                        if not target.startswith('#'):
                            referenced_pages.add(target)
                            
                    # Check Mermaid
                    in_mermaid = False
                    for line in content.split('\n'):
                        if line.startswith('```mermaid'):
                            in_mermaid = True
                        elif line.startswith('```') and in_mermaid:
                            in_mermaid = False
                        elif in_mermaid:
                            if '[[' in line and ']]' in line:
                                self.report.invalid_mermaid.append((file, "Wiki link inside Mermaid breaks renderer"))
                                
                    # Missing Metadata / Sections
                    if "<!-- AUTO-GENERATED START -->" in content and "_Unspecified_" in content:
                        self.report.missing_metadata.append(file)

        # Cross validate links
        for root, _, files in os.walk(docs_dir):
            if '.obsidian' in root: continue
            for file in files:
                if file.endswith('.md'):
                    filepath = os.path.join(root, file)
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read()
                    for match in re.finditer(r'\[\[(.*?)\]\]', content):
                        target = match.group(1).split('|')[0].strip()
                        if target not in all_pages and not target.startswith('#'):
                            self.report.broken_wiki_links.append((file, target))

        # Orphans
        orphans = all_pages - referenced_pages
        # Filter out special pages that don't need incoming links strictly, or just report them
        self.report.orphaned_pages = list(orphans)
        
        # Duplicates
        for title, files in page_titles.items():
            if len(files) > 1:
                self.report.duplicate_titles.append((title, files))
                
        return not self.report.has_errors()
