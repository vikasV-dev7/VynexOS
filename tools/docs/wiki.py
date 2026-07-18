import re
from typing import List

def auto_link_text(text: str, known_entities: List[str]) -> str:
    """
    Scans text for known_entities and replaces them with [[Entity]] wiki links.
    Avoids double linking if already linked or if inside a mermaid block.
    """
    # Temporarily hide mermaid blocks to avoid linking inside them
    mermaid_blocks = []
    def hide_mermaid(match):
        mermaid_blocks.append(match.group(0))
        return f"__MERMAID_BLOCK_{len(mermaid_blocks)-1}__"
        
    text = re.sub(r'```mermaid.*?```', hide_mermaid, text, flags=re.DOTALL)
    
    for entity in known_entities:
        # Simple negative lookbehind/lookahead to avoid [[Entity]] matching Entity
        pattern = r'(?<!\[\[)\b' + re.escape(entity) + r'\b(?!\]\])'
        text = re.sub(pattern, f"[[{entity}]]", text)
        
    # Restore mermaid blocks
    for i, block in enumerate(mermaid_blocks):
        text = text.replace(f"__MERMAID_BLOCK_{i}__", block)
        
    return text
