import os
import json
import datetime
from typing import List, Dict
from .plugins import IDocumentationCache
from .model import IR_VERSION, SCHEMA_VERSION

class FileHashCache(IDocumentationCache):
    def __init__(self, generator_version: str, parser_version: str):
        self._generator_version = generator_version
        self._parser_version = parser_version
        self.cache_file = ""
        self.data = {
            "metadata": {
                "ir_version": IR_VERSION,
                "schema_version": SCHEMA_VERSION,
                "generator_version": self._generator_version,
                "parser_version": self._parser_version,
                "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat()
            },
            "entities": {}
        }
        self.metrics_cached = 0
        self.metrics_regenerated = 0

    @property
    def name(self) -> str: return "FileHashCache"
    @property
    def version(self) -> str: return "1.0.0"
    @property
    def supported_ir_versions(self) -> List[str]: return [IR_VERSION]

    def initialize(self, cache_file: str):
        self.cache_file = cache_file
        if os.path.exists(cache_file):
            try:
                with open(cache_file, 'r', encoding='utf-8') as f:
                    loaded = json.load(f)
                
                # Global Invalidation
                meta = loaded.get("metadata", {})
                if (meta.get("ir_version") != IR_VERSION or
                    meta.get("schema_version") != SCHEMA_VERSION or
                    meta.get("generator_version") != self._generator_version or
                    meta.get("parser_version") != self._parser_version):
                    # Invalidate entirely
                    return
                
                self.data = loaded
            except Exception:
                pass # Corrupt cache, ignore

    def is_valid(self, source_file: str, entity_hash: str) -> bool:
        entry = self.data["entities"].get(source_file)
        if not entry: return False
        
        if entry.get("entity_hash") == entity_hash:
            self.metrics_cached += 1
            return True
            
        self.metrics_regenerated += 1
        return False

    def update(self, source_file: str, entity_hash: str):
        self.data["entities"][source_file] = {
            "entity_hash": entity_hash,
            "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat()
        }

    def save(self):
        self.data["metadata"]["timestamp"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        with open(self.cache_file, 'w', encoding='utf-8') as f:
            json.dump(self.data, f, indent=4)

    def clean(self):
        if os.path.exists(self.cache_file):
            os.remove(self.cache_file)
        self.data["entities"] = {}
