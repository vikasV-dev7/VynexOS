import os
import re

docs_dir = r'C:\Users\User\Desktop\Git Projects\VynexOS\docs'

def find_broken_links():
    print('--- Checking Links ---')
    all_files = set()
    for root, _, files in os.walk(docs_dir):
        if '.obsidian' in root: continue
        for file in files:
            if file.endswith('.md'):
                all_files.add(file[:-3])
    
    for root, _, files in os.walk(docs_dir):
        if '.obsidian' in root: continue
        for file in files:
            if file.endswith('.md'):
                path = os.path.join(root, file)
                with open(path, 'r', encoding='utf-8') as f:
                    content = f.read()
                    links = re.findall(r'\[\[(.*?)\]\]', content)
                    for link in links:
                        target = link.split('|')[0].strip()
                        if target not in all_files and not target.startswith('#'):
                            print(f'Broken link in {file}: [[{target}]]')

find_broken_links()
