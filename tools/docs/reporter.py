from typing import List
from .plugins import IDocumentationReporter, IDocumentationCache, IDocumentationValidator
from .model import DocumentEntity, IR_VERSION
import time

class ConsoleReporter(IDocumentationReporter):
    def __init__(self):
        self.metrics = {}

    @property
    def name(self) -> str: return "ConsoleReporter"
    @property
    def version(self) -> str: return "1.0.0"
    @property
    def supported_ir_versions(self) -> List[str]: return [IR_VERSION]

    def collect_metrics(self, entities: List[DocumentEntity], cache: IDocumentationCache, validator: IDocumentationValidator):
        self.metrics["interfaces"] = sum(1 for e in entities if e.is_interface)
        self.metrics["services"] = sum(1 for e in entities if not e.is_interface)
        self.metrics["methods"] = sum(len(e.methods) for e in entities)
        
        # Metadata coverage
        fields_checked = 0
        fields_present = 0
        for e in entities:
            meta = e.metadata
            fields = [meta.purpose, meta.responsibilities, meta.thread_safety, meta.ownership, meta.failure_handling]
            fields_checked += len(fields)
            fields_present += sum(1 for f in fields if f)
            
        self.metrics["metadata_coverage"] = (fields_present / fields_checked * 100) if fields_checked else 0
        self.metrics["cached_pages"] = getattr(cache, 'metrics_cached', 0)
        self.metrics["regenerated_pages"] = getattr(cache, 'metrics_regenerated', 0)
        
        # Validation
        if hasattr(validator, 'report') and validator.report:
            self.metrics["validation_errors"] = sum(len(x) for x in [
                validator.report.broken_wiki_links,
                validator.report.broken_md_links,
                validator.report.orphaned_pages,
                validator.report.duplicate_titles,
                validator.report.invalid_mermaid,
                validator.report.missing_metadata
            ])
        else:
            self.metrics["validation_errors"] = 0

    def print_report(self):
        print("\n=== Engineering Metrics Report ===")
        print(f"Interfaces Documented: {self.metrics.get('interfaces', 0)}")
        print(f"Services Documented:   {self.metrics.get('services', 0)}")
        print(f"Methods Documented:    {self.metrics.get('methods', 0)}")
        print(f"Metadata Coverage:     {self.metrics.get('metadata_coverage', 0):.1f}%")
        print(f"Cached Pages:          {self.metrics.get('cached_pages', 0)}")
        print(f"Regenerated Pages:     {self.metrics.get('regenerated_pages', 0)}")
        print(f"Validation Errors:     {self.metrics.get('validation_errors', 0)}")
        print("==================================\n")
