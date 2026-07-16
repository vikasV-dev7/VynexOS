import os
import re
from pathlib import Path

def ensure_dir(path: str):
    Path(path).mkdir(parents=True, exist_ok=True)

def write_preserved(filepath: str, new_content: str):
    """
    Writes new_content to filepath. If the file already exists, it preserves
    anything outside the <!-- AUTO-GENERATED START --> and <!-- AUTO-GENERATED END --> markers.
    If no markers exist, it overwrites the whole file but warns.
    """
    START_MARKER = "<!-- AUTO-GENERATED START -->"
    END_MARKER = "<!-- AUTO-GENERATED END -->"
    
    if not os.path.exists(filepath):
        # Create fresh file with markers
        content = f"{START_MARKER}\n{new_content}\n{END_MARKER}\n"
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True # Created

    with open(filepath, 'r', encoding='utf-8') as f:
        original = f.read()

    start_idx = original.find(START_MARKER)
    end_idx = original.find(END_MARKER)

    if start_idx != -1 and end_idx != -1 and end_idx > start_idx:
        # Preserve manual edits
        before = original[:start_idx + len(START_MARKER)]
        after = original[end_idx:]
        final_content = f"{before}\n{new_content}\n{after}"
    else:
        # No markers found, just wrap it
        final_content = f"{START_MARKER}\n{new_content}\n{END_MARKER}\n"

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(final_content)
    return False # Updated

def extract_comments(text: str) -> str:
    """Extracts block and line comments from C++ code."""
    # Simple regex to get // and /* */ comments
    comments = []
    for match in re.finditer(r'//(.*?)$|/\*(.*?)\*/', text, re.DOTALL | re.MULTILINE):
        if match.group(1):
            comments.append(match.group(1).strip())
        elif match.group(2):
            comments.append(match.group(2).strip())
    return " ".join(comments)
