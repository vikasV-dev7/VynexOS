import os
import sys
import argparse

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')))

from tools.docs.model import IR_VERSION
from tools.docs.plugins import PluginRegistry
from tools.docs.clang_parser import ClangASTParser
from tools.docs.regex_parser import RegexParser
from tools.docs.renderer import MarkdownRenderer
from tools.docs.validator import DocumentationValidator
from tools.docs.indexer import JsonSearchIndexer
from tools.docs.cache import FileHashCache
from tools.docs.reporter import ConsoleReporter
from tools.docs.wiki import auto_link_text
from tools.docs.utils import ensure_dir

def build_registry(repo_root: str) -> PluginRegistry:
    registry = PluginRegistry(IR_VERSION)
    
    # 1. Register Cache
    # Note: version should ideally be extracted from package, hardcoding for now
    registry.register(FileHashCache("2.1.0", "1.1.0"))
    
    # 2. Register Parser (ClangAST is canonical)
    try:
        registry.register(ClangASTParser(repo_root))
    except RuntimeError as e:
        print(f"[ERROR] Canonical Parser Failed: {e}", file=sys.stderr)
        print("[WARNING] Failing over to RegexParser only for development compatibility.", file=sys.stderr)
        registry.register(RegexParser())
        
    # 3. Register Renderer
    registry.register(MarkdownRenderer(registry.parsers[0].name))
    
    # 4. Register Validator
    registry.register(DocumentationValidator())
    
    # 5. Register Indexer
    registry.register(JsonSearchIndexer())
    
    # 6. Register Reporter
    registry.register(ConsoleReporter())
    
    return registry

def main():
    parser = argparse.ArgumentParser(description="VynexOS Documentation Platform")
    parser.add_argument("--generate", action="store_true", help="Generate documentation")
    parser.add_argument("--validate", action="store_true", help="Run validation only")
    parser.add_argument("--report", action="store_true", help="Produce engineering metrics only")
    parser.add_argument("--clean", action="store_true", help="Remove generated artifacts and cache")
    parser.add_argument("--force-rebuild", action="store_true", help="Ignore cache")
    args = parser.parse_args()

    # Default if nothing is passed
    if not (args.generate or args.validate or args.report or args.clean):
        args.generate = True

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
    docs_dir = os.path.join(repo_root, "docs")
    cache_path = os.path.join(docs_dir, ".docs_cache.json")
    ensure_dir(docs_dir)
    
    registry = build_registry(repo_root)
    cache = registry.caches[0]
    validator = registry.validators[0]
    reporter = registry.reporters[0]
    indexer = registry.indexers[0]
    doc_parser = registry.parsers[0]
    renderer = registry.renderers[0]

    if args.clean:
        print("Cleaning documentation cache...")
        cache.clean()
        sys.exit(0)

    # Initialize Cache
    if not args.force_rebuild:
        cache.initialize(cache_path)

    # Parsing is required for generate, report, and indexer
    interfaces, services = [], []
    if args.generate or args.report:
        print(f"Scanning repository using {doc_parser.name}...")
        interfaces, services = doc_parser.scan_repository(repo_root)
        all_entities = interfaces + services
    
    if args.generate:
        print("Rendering markdown...")
        known_entities = [e.name for e in all_entities]
        
        for category, entities in [("Interfaces", interfaces), ("Services", services)]:
            out_dir = os.path.join(docs_dir, category)
            ensure_dir(out_dir)
            
            for entity in entities:
                entity_hash = entity.calculate_hash()
                if not args.force_rebuild and cache.is_valid(entity.source_path, entity_hash):
                    continue # Skip, unchanged
                    
                raw_md = renderer.render(entity)
                linked_md = auto_link_text(raw_md, known_entities)
                filepath = os.path.join(out_dir, f"{entity.name}.md")
                final_md = renderer.merge_preserved(filepath, linked_md)
                
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(final_md)
                    
                cache.update(entity.source_path, entity_hash)
                
        print("Building Search Index...")
        indexer.build_index(all_entities, docs_dir)
        cache.save()
        
    if args.validate or args.generate:
        print("Running Documentation Validation Pass...")
        is_valid = validator.validate(docs_dir)
        if hasattr(validator, 'report'):
            validator.report.print_report()
        if not is_valid:
            print("[WARNING] Documentation generated with validation errors.")
            
    if args.report:
        print("Generating Engineering Metrics...")
        reporter.collect_metrics(all_entities, cache, validator)
        reporter.print_report()

if __name__ == "__main__":
    main()
