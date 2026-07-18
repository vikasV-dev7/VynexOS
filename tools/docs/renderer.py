import os
import datetime
from .model import Interface, Service, DocumentEntity, IR_VERSION, SCHEMA_VERSION
from .plugins import IDocumentationRenderer

GENERATOR_VERSION = "2.1.0"
FORMAT_VERSION = SCHEMA_VERSION

class MarkdownRenderer(IDocumentationRenderer):
    @property
    def name(self) -> str: return "MarkdownRenderer"
    @property
    def version(self) -> str: return "1.1.0"
    @property
    def supported_ir_versions(self) -> list[str]: return [IR_VERSION]

    def __init__(self, parser_name: str = "Unknown"):
        self.parser_name = parser_name

    def render(self, entity: DocumentEntity) -> str:
        if entity.is_interface:
            return self.render_interface(entity)
        return self.render_service(entity)

    def render_interface(self, entity: Interface) -> str:
        lines = []
        lines.append(f"# {entity.name}")
        lines.append("")
        
        meta = entity.metadata
        
        lines.append("## Why the interface exists & Architectural role")
        lines.append(meta.purpose if meta.purpose else "_Not explicitly documented._")
        lines.append("")
        
        lines.append("## Lifetime & Ownership expectations")
        lines.append(f"**Lifetime**: {meta.lifetime if meta.lifetime else '_Unspecified_'}")
        lines.append(f"**Ownership**: {meta.ownership if meta.ownership else '_Unspecified_'}")
        lines.append("")
        
        lines.append("## Threading guarantees & Failure behaviour")
        lines.append(f"**Thread Safety**: {meta.thread_safety if meta.thread_safety else '_Unspecified_'}")
        lines.append(f"**Failure Handling**: {meta.failure_handling if meta.failure_handling else '_Unspecified_'}")
        lines.append("")
        
        lines.append("## Extension guidance")
        lines.append(meta.extension_points if meta.extension_points else "_No explicit extension points defined._")
        lines.append("")
        
        lines.append("## Public API")
        if not entity.methods:
            lines.append("No public methods extracted.")
        for method in entity.methods:
            lines.append(f"- `{method.signature}`")
        lines.append("")
        
        self._append_footer(lines, entity)
        return "\n".join(lines)

    def render_service(self, entity: Service) -> str:
        lines = []
        lines.append(f"# {entity.name}")
        lines.append("")
        
        meta = entity.metadata
        
        lines.append("## Purpose & Responsibilities")
        if meta.purpose or meta.responsibilities:
            lines.append(meta.purpose if meta.purpose else "")
            lines.append(meta.responsibilities if meta.responsibilities else "")
        else:
            lines.append("_Not explicitly documented._")
        lines.append("")
        
        lines.append("## Dependencies & Interaction with CompositionRoot")
        lines.append("_Constructor injected dependencies (Refer to Source Code)._")
        lines.append("")
        
        lines.append("## Failure recovery & Performance considerations")
        lines.append(f"**Failure Recovery**: {meta.failure_handling if meta.failure_handling else '_Unspecified_'}")
        lines.append(f"**Performance**: {meta.performance if meta.performance else '_Unspecified_'}")
        lines.append("")
        
        lines.append("## Extension points")
        lines.append(meta.extension_points if meta.extension_points else "_No explicit extension points defined._")
        lines.append("")
        
        lines.append("## Public API")
        if not entity.methods:
            lines.append("No public methods extracted.")
        for method in entity.methods:
            lines.append(f"- `{method.signature}`")
        lines.append("")

        self._append_footer(lines, entity)
        return "\n".join(lines)

    def _append_footer(self, lines: list, entity: DocumentEntity):
        lines.append("---")
        lines.append("### Generator Metadata")
        lines.append(f"- **Generation Timestamp**: {datetime.datetime.now(datetime.timezone.utc).isoformat()}")
        lines.append(f"- **Generator Version**: {GENERATOR_VERSION}")
        lines.append(f"- **Documentation Schema**: {FORMAT_VERSION}")
        lines.append(f"- **IR Version**: {entity.ir_version}")
        lines.append(f"- **Parser**: {self.parser_name}")
        lines.append(f"- **Source File**: `{entity.source_path}`")
        lines.append("")

    def merge_preserved(self, filepath: str, generated_markdown: str) -> str:
        """
        Merges the generated markdown with any manually curated markdown in the file.
        Uses <!-- AUTO-GENERATED START --> and <!-- AUTO-GENERATED END -->.
        """
        START_MARKER = "<!-- AUTO-GENERATED START -->"
        END_MARKER = "<!-- AUTO-GENERATED END -->"
        
        new_block = f"{START_MARKER}\n{generated_markdown}\n{END_MARKER}\n"
        
        if not os.path.exists(filepath):
            return new_block
            
        with open(filepath, 'r', encoding='utf-8') as f:
            original = f.read()
            
        start_idx = original.find(START_MARKER)
        end_idx = original.find(END_MARKER)
        
        if start_idx != -1 and end_idx != -1 and end_idx > start_idx:
            before = original[:start_idx]
            after = original[end_idx + len(END_MARKER):]
            # Ensure newline separation
            if before and not before.endswith('\n'): before += '\n'
            if after and not after.startswith('\n'): after = '\n' + after
            return f"{before}{new_block}{after}"
        else:
            return f"{new_block}\n{original}"
