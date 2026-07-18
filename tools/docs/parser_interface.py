from abc import ABC, abstractmethod
from typing import List, Tuple
from .model import Interface, Service

class IDocumentationParser(ABC):
    """
    Abstract interface for all documentation parsers.
    Parsers are responsible for reading C++ files and emitting
    the parser-independent intermediate models.
    """
    
    @abstractmethod
    def parse_file(self, filepath: str) -> List[dict]:
        """
        Parses a single C++ source file.
        Returns a list of parsed class/interface dictionaries which are then mapped to models.
        """
        pass

    @abstractmethod
    def scan_repository(self, repo_root: str) -> Tuple[List[Interface], List[Service]]:
        """
        Scans the entire repository and returns the intermediate models.
        """
        pass
