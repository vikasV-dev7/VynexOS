import os
import json
from typing import List
from .model import DocumentEntity, IR_VERSION, asdict
from .plugins import IDocumentationIndexer

class JsonSearchIndexer(IDocumentationIndexer):
    @property
    def name(self) -> str: return "JsonSearchIndexer"
    @property
    def version(self) -> str: return "1.0.0"
    @property
    def supported_ir_versions(self) -> List[str]: return [IR_VERSION]

    def build_index(self, entities: List[DocumentEntity], output_dir: str):
        index = []
        for e in entities:
            entry = {
                "Identifier": e.name,
                "Display Name": e.name,
                "Category": "Interface" if e.is_interface else "Service",
                "Source File": e.source_path,
                "Namespace": e.namespace,
                "Parent": e.base_classes,
                "Public API": [m.signature for m in e.methods],
                "Metadata": {
                    "Purpose": e.metadata.purpose,
                    "Responsibilities": e.metadata.responsibilities,
                    "Ownership": e.metadata.ownership,
                    "Lifetime": e.metadata.lifetime,
                    "Thread Safety": e.metadata.thread_safety,
                    "Failure Handling": e.metadata.failure_handling,
                    "Performance": e.metadata.performance,
                    "Extension Points": e.metadata.extension_points
                },
                "Related Documents": [] # Stub for future graph mapping
            }
            index.append(entry)
            
        output_path = os.path.join(output_dir, "documentation_index.json")
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(index, f, indent=4)
