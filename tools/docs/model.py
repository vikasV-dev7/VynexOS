from dataclasses import dataclass, field, asdict
from typing import List, Dict, Optional
import hashlib
import json

IR_VERSION = "1.0"
SCHEMA_VERSION = "1.1"

@dataclass
class Metadata:
    purpose: Optional[str] = None
    responsibilities: Optional[str] = None
    ownership: Optional[str] = None
    lifetime: Optional[str] = None
    thread_safety: Optional[str] = None
    failure_handling: Optional[str] = None
    performance: Optional[str] = None
    extension_points: Optional[str] = None
    raw_doxygen: str = ""

@dataclass
class Method:
    name: str
    signature: str
    return_type: str
    is_virtual: bool
    is_noexcept: bool
    is_nodiscard: bool
    metadata: Metadata = field(default_factory=Metadata)

@dataclass
class DocumentEntity:
    name: str
    namespace: str
    source_path: str
    is_interface: bool
    base_classes: List[str] = field(default_factory=list)
    methods: List[Method] = field(default_factory=list)
    metadata: Metadata = field(default_factory=Metadata)
    ir_version: str = IR_VERSION
    
    def calculate_hash(self) -> str:
        """Calculates a deterministic hash representing the IR state of this entity."""
        d = asdict(self)
        d_str = json.dumps(d, sort_keys=True)
        return hashlib.sha256(d_str.encode('utf-8')).hexdigest()

@dataclass
class Interface(DocumentEntity):
    def __post_init__(self):
        self.is_interface = True

@dataclass
class Service(DocumentEntity):
    def __post_init__(self):
        self.is_interface = False
