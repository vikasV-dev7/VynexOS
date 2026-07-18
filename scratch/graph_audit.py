import os, re
docs_dir = 'docs'
all_files = []
for r, d, f in os.walk(docs_dir):
    if '.obsidian' in r: continue
    for file in f:
        if file.endswith('.md'):
            all_files.append(os.path.join(r, file))

links = {}
pages = set()

for f in all_files:
    page_name = os.path.basename(f)[:-3]
    pages.add(page_name)
    links[page_name] = {'out': set(), 'in': set(), 'file': f}

for f in all_files:
    page_name = os.path.basename(f)[:-3]
    with open(f, 'r', encoding='utf-8') as file:
        content = file.read()
    for match in re.finditer(r'\[\[(.*?)\]\]', content):
        target = match.group(1).split('|')[0].strip()
        if target.startswith('#'): continue
        links[page_name]['out'].add(target)
        if target not in links:
            links[target] = {'out': set(), 'in': set()}
        links[target]['in'].add(page_name)

orphans = [p for p in pages if len(links.get(p, {}).get('in', [])) == 0 and len(links.get(p, {}).get('out', [])) == 0]
no_incoming = [p for p in pages if len(links.get(p, {}).get('in', [])) == 0 and len(links.get(p, {}).get('out', [])) > 0]
no_outgoing = [p for p in pages if len(links.get(p, {}).get('in', [])) > 0 and len(links.get(p, {}).get('out', [])) == 0]

print(f'Total Pages: {len(pages)}')
print(f'Total Wiki Links: {sum(len(l.get("out", set())) for l in links.values())}')
print(f'Strict Orphans (no in/out): {len(orphans)}')
print(f'No Incoming Links: {len(no_incoming)}')
print(f'Dead Ends (no outgoing): {len(no_outgoing)}')

print('\nPages with NO INCOMING:')
for p in sorted(no_incoming): print(f'- {p}')

print('\nSTRICT ORPHANS:')
for p in sorted(orphans): print(f'- {p}')
