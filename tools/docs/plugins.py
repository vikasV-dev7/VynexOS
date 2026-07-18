from abc import ABC, abstractmethod
from typing import List, Tuple, Dict, Optional
from .model import DocumentEntity, Interface, Service

class IPlugin(ABC):
    @property
    @abstractmethod
    def name(self) -> str: pass
    
    @property
    @abstractmethod
    def version(self) -> str: pass
    
    @property
    @abstractmethod
    def supported_ir_versions(self) -> List[str]: pass

class IDocumentationParser(IPlugin):
    @abstractmethod
    def scan_repository(self, repo_root: str) -> Tuple[List[Interface], List[Service]]:
        pass

class IDocumentationRenderer(IPlugin):
    @abstractmethod
    def render(self, entity: DocumentEntity) -> str:
        pass
        
    @abstractmethod
    def merge_preserved(self, filepath: str, generated_markdown: str) -> str:
        pass

class IDocumentationValidator(IPlugin):
    @abstractmethod
    def validate(self, docs_dir: str) -> bool:
        pass

class IDocumentationIndexer(IPlugin):
    @abstractmethod
    def build_index(self, entities: List[DocumentEntity], output_dir: str):
        pass

class IDocumentationCache(IPlugin):
    @abstractmethod
    def initialize(self, cache_file: str):
        pass
        
    @abstractmethod
    def is_valid(self, source_file: str, entity_hash: str) -> bool:
        pass
        
    @abstractmethod
    def update(self, source_file: str, entity_hash: str):
        pass
        
    @abstractmethod
    def save(self):
        pass
        
    @abstractmethod
    def clean(self):
        pass

class IDocumentationReporter(IPlugin):
    @abstractmethod
    def collect_metrics(self, entities: List[DocumentEntity], cache: IDocumentationCache, validator: IDocumentationValidator):
        pass
        
    @abstractmethod
    def print_report(self):
        pass

class PluginRegistry:
    def __init__(self, current_ir_version: str):
        self.current_ir_version = current_ir_version
        self.parsers: List[IDocumentationParser] = []
        self.renderers: List[IDocumentationRenderer] = []
        self.validators: List[IDocumentationValidator] = []
        self.indexers: List[IDocumentationIndexer] = []
        self.reporters: List[IDocumentationReporter] = []
        self.caches: List[IDocumentationCache] = []

    def register(self, plugin: IPlugin):
        if self.current_ir_version not in plugin.supported_ir_versions:
            raise RuntimeError(f"Plugin {plugin.name} v{plugin.version} does not support IR {self.current_ir_version}")
            
        if isinstance(plugin, IDocumentationParser): self.parsers.append(plugin)
        elif isinstance(plugin, IDocumentationRenderer): self.renderers.append(plugin)
        elif isinstance(plugin, IDocumentationValidator): self.validators.append(plugin)
        elif isinstance(plugin, IDocumentationIndexer): self.indexers.append(plugin)
        elif isinstance(plugin, IDocumentationReporter): self.reporters.append(plugin)
        elif isinstance(plugin, IDocumentationCache): self.caches.append(plugin)
        else:
            raise RuntimeError(f"Unknown plugin type: {type(plugin)}")
