import os
import sys
from pathlib import Path
from typing import List, Tuple
from .model import Interface, Service, Method, Metadata, IR_VERSION
from .plugins import IDocumentationParser

class ClangASTParser(IDocumentationParser):
    @property
    def name(self) -> str: return "ClangASTParser"
    @property
    def version(self) -> str: return "1.1.0"
    @property
    def supported_ir_versions(self) -> list[str]: return [IR_VERSION]

    def __init__(self, repo_root: str):
        self.repo_root = repo_root
        self.compile_commands_path = os.path.join(repo_root, "build", "compile_commands.json")
        
        try:
            import clang.cindex
            self.clang = clang.cindex
        except ImportError:
            raise RuntimeError("libclang is not installed. Run `pip install libclang`.")

        if not os.path.exists(self.compile_commands_path):
            raise RuntimeError(
                f"Missing compile_commands.json at {self.compile_commands_path}. "
                "Ensure CMake is configured with CMAKE_EXPORT_COMPILE_COMMANDS=ON."
            )

        self.index = self.clang.Index.create()
        self.compilation_database = self.clang.CompilationDatabase.fromDirectory(os.path.dirname(self.compile_commands_path))

    def _parse_doxygen_tags(self, raw_comment: str) -> Metadata:
        meta = Metadata(raw_doxygen=raw_comment)
        if not raw_comment: return meta
        
        # Simple extraction for Doxygen @tags
        lines = raw_comment.split('\n')
        current_tag = None
        current_text = []

        def commit_tag():
            if current_tag and current_text:
                text = " ".join(current_text).strip()
                if current_tag == "@Purpose": meta.purpose = text
                elif current_tag == "@Responsibilities": meta.responsibilities = text
                elif current_tag == "@Ownership": meta.ownership = text
                elif current_tag == "@Lifetime": meta.lifetime = text
                elif current_tag == "@ThreadSafety": meta.thread_safety = text
                elif current_tag == "@FailureHandling": meta.failure_handling = text
                elif current_tag == "@Performance": meta.performance = text
                elif current_tag == "@ExtensionPoints": meta.extension_points = text

        for line in lines:
            line = line.strip().lstrip('*').lstrip('/')
            if '@' in line:
                commit_tag()
                parts = line.split(' ', 1)
                current_tag = parts[0].strip()
                current_text = [parts[1].strip()] if len(parts) > 1 else []
            elif current_tag:
                current_text.append(line.strip())
        
        commit_tag()
        return meta

    def parse_file(self, filepath: str) -> List[dict]:
        cmds = self.compilation_database.getCompileCommands(filepath)
        args = []
        if cmds:
            for cmd in cmds:
                for arg in cmd.arguments:
                    if arg != cmd.compiler and arg != filepath:
                        args.append(arg)
        else:
            args = ['-std=c++2b', '-x', 'c++']
        
        tu = self.index.parse(filepath, args=args)
        
        results = []
        namespace = "vynexos" # Default

        def visit(node):
            nonlocal namespace
            if node.kind == self.clang.CursorKind.NAMESPACE:
                namespace = node.spelling
            elif node.kind in [self.clang.CursorKind.CLASS_DECL, self.clang.CursorKind.STRUCT_DECL]:
                if node.is_definition() and str(node.location.file) == filepath:
                    class_name = node.spelling
                    base_classes = []
                    methods = []
                    
                    for child in node.get_children():
                        if child.kind == self.clang.CursorKind.CXX_BASE_SPECIFIER:
                            base_classes.append(child.spelling)
                        elif child.kind == self.clang.CursorKind.CXX_METHOD:
                            methods.append(Method(
                                name=child.spelling,
                                signature=child.displayname,
                                return_type=child.result_type.spelling,
                                is_virtual=child.is_virtual_method(),
                                is_noexcept=False, # Clang bindings don't expose this easily in older versions, but could be parsed
                                is_nodiscard=False,
                                metadata=self._parse_doxygen_tags(child.raw_comment)
                            ))

                    metadata = self._parse_doxygen_tags(node.raw_comment)
                    is_interface = class_name.startswith('I') and class_name[1].isupper()

                    results.append({
                        'name': class_name,
                        'namespace': namespace,
                        'is_interface': is_interface,
                        'base_classes': base_classes,
                        'methods': methods,
                        'metadata': metadata,
                        'source_path': filepath.replace('\\', '/')
                    })

            for c in node.get_children():
                visit(c)

        visit(tu.cursor)
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
