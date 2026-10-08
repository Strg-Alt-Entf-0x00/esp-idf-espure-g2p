import json
import pathlib
import sys

class TrieNode:
    def __init__(self, byte_val):
        self.byte_val = byte_val
        self.is_end = False
        self.children = {}
        self.next_state = 0
        self.child_count = 0

def build_trie():
    gt_path = pathlib.Path(__file__).parent.parent / 'data' / 'dictsource' / 'de_DE_roots.json'
    if not gt_path.exists():
        print(f"Error: {gt_path} not found.")
        sys.exit(1)
        
    with open(gt_path, 'r', encoding='utf-8') as f:
        words = json.load(f)
        
    # We also add prefixes and suffixes to the trie so the C engine knows them
    prefixes = ["ab", "an", "auf", "aus", "be", "bei", "da", "dar", "durch", "ein", "emp", 
                "ent", "er", "fort", "ge", "her", "hin", "hinter", "mit", "nach", "nieder", 
                "ober", "unter", "über", "um", "un", "ur", "ver", "vor", "weg", "wider", 
                "wieder", "zer", "zu", "zurecht", "zurück", "zusammen", "zwischen",
                "bundes", "kinder", "haupt", "sonder", "super", "halb", "lieblings"]
                
    suffixes = ["bar", "chen", "ei", "en", "end", "er", "haft", "heit", "ie", "ig", "in", 
                "isch", "keit", "lein", "lich", "ling", "nis", "sal", "sam", "schaft", 
                "tum", "ung", "werk", "los", "voll", "mäßig", "innen"]
                
    words.extend(prefixes)
    words.extend(suffixes)
    
    # Remove duplicates and sort
    words = sorted(list(set(w.lower() for w in words)))
    
    root = TrieNode(0)
    for w in words:
        curr = root
        for b in w.encode('utf-8'):
            if b not in curr.children:
                curr.children[b] = TrieNode(b)
            curr = curr.children[b]
        curr.is_end = True
        
    # Flatten via BFS
    flattened = [root]
    idx = 0
    while idx < len(flattened):
        curr = flattened[idx]
        sorted_keys = sorted(curr.children.keys())
        curr.next_state = len(flattened)
        curr.child_count = len(sorted_keys)
        for k in sorted_keys:
            flattened.append(curr.children[k])
        idx += 1
        
    print(f"Built Trie with {len(flattened)} nodes for {len(words)} unique roots/affixes.")
    
    # Generate C code
    out_dir = pathlib.Path(__file__).parent.parent / 'data' / 'dict'
    out_dir.mkdir(parents=True, exist_ok=True)
    out_c = out_dir / 'de_DE_morph_trie.c'
    out_h = out_dir / 'de_DE_morph_trie.h'
    
    with open(out_h, 'w', encoding='utf-8') as f:
        f.write("#ifndef DE_DE_MORPH_TRIE_H\n#define DE_DE_MORPH_TRIE_H\n\n")
        f.write("#include <stdint.h>\n#include <stdbool.h>\n\n")
        f.write("typedef struct {\n")
        f.write("    uint8_t byte_val;\n")
        f.write("    bool is_end;\n")
        f.write("    uint32_t next_state;\n")
        f.write("    uint16_t child_count;\n")
        f.write("} MorphTrieNode;\n\n")
        f.write(f"extern const MorphTrieNode de_DE_morph_trie[{len(flattened)}];\n\n")
        f.write("#endif // DE_DE_MORPH_TRIE_H\n")
        
    with open(out_c, 'w', encoding='utf-8') as f:
        f.write('#include "de_DE_morph_trie.h"\n\n')
        f.write(f"const MorphTrieNode de_DE_morph_trie[{len(flattened)}] = {{\n")
        for node in flattened:
            is_end_str = "true" if node.is_end else "false"
            f.write(f"    {{ {node.byte_val}, {is_end_str}, {node.next_state}, {node.child_count} }},\n")
        f.write("};\n")
        
    print(f"Successfully wrote C header to {out_h}")
    print(f"Successfully wrote C source to {out_c}")
    print(f"Memory footprint: {len(flattened) * 8} bytes ({len(flattened) * 8 / 1024:.2f} KB)")

if __name__ == "__main__":
    build_trie()
