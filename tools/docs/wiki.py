import re

def create_link(target: str, display: str = None) -> str:
    if display and display != target:
        return f"[[{target}|{display}]]"
    return f"[[{target}]]"

def auto_link_text(text: str, known_entities: list) -> str:
    """
    Scans text for known_entities and replaces them with [[Entity]] wiki links.
    Avoids double linking if already linked.
    """
    for entity in known_entities:
        # Simple negative lookbehind/lookahead to avoid [[Entity]] matching Entity
        pattern = r'(?<!\[\[)\b' + re.escape(entity) + r'\b(?!\]\])'
        text = re.sub(pattern, f"[[{entity}]]", text)
    return text
