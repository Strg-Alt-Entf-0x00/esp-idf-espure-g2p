import json
import pathlib

def build_root_dictionary():
    gt_path = pathlib.Path(__file__).parent.parent / 'data' / 'ground_truth' / 'de_DE_ground_truth.json'
    with open(gt_path, 'r', encoding='utf-8') as f:
        gt = json.load(f)
        
    words = list(gt.keys())
    # Sort by length so we process shorter words first
    words.sort(key=len)
    
    roots = set()
    compounds = {}
    
    prefixes = {"ab", "an", "auf", "aus", "be", "bei", "da", "dar", "durch", "ein", "emp", 
                "ent", "er", "fort", "ge", "her", "hin", "hinter", "mit", "nach", "nieder", 
                "ober", "unter", "über", "um", "un", "ur", "ver", "vor", "weg", "wider", 
                "wieder", "zer", "zu", "zurecht", "zurück", "zusammen", "zwischen"}
                
    suffixes = {"bar", "chen", "ei", "en", "end", "er", "haft", "heit", "ie", "ig", "in", 
                "isch", "keit", "lein", "lich", "ling", "nis", "sal", "sam", "schaft", 
                "tum", "ung", "werk", "los", "voll", "mäßig", "innen"}
                
    linking_elements = {"s", "es", "n", "en", "e", "er"}

    def can_split(word):
        n = len(word)
        # dp[i] stores the maximum number of valid roots used to form word[:i]
        # -1 means cannot be formed.
        dp = [-1] * (n + 1)
        dp[0] = 0
        
        for i in range(1, n + 1):
            for j in range(i):
                if dp[j] != -1:
                    part = word[j:i].lower()
                    
                    if part in prefixes or part in suffixes or (j > 0 and i < n and part in linking_elements):
                        # Used an affix/linker, root count doesn't increase
                        if dp[j] > dp[i]:
                            dp[i] = dp[j]
                    elif len(part) >= 3 and part in lower_roots:
                        # Used a root (must be at least 3 chars), root count increases
                        if dp[j] + 1 > dp[i]:
                            dp[i] = dp[j] + 1
                            
        # It's a valid compound ONLY if it can be formed entirely AND uses at least 1 root
        # Actually, if it uses exactly 1 root, it's just a derived word (e.g. root+suffix), which is fine.
        # But wait, if we want to extract pure roots, a derived word SHOULD be split into root+suffix, so it's a compound.
        # So as long as dp[n] >= 1, it can be split.
        return dp[n] >= 1

    lower_roots = set()
    
    print(f"Extracting true roots from {len(words)} words...")
    for w in words:
        if len(w) <= 3:
            # Too short to be a compound of multiple roots
            roots.add(w)
            lower_roots.add(w.lower())
            continue
            
        if can_split(w):
            compounds[w] = True
        else:
            roots.add(w)
            lower_roots.add(w.lower())
            
    print(f"Found {len(roots)} basic roots and {len(compounds)} compounds/derived words.")
    
    out_path = pathlib.Path(__file__).parent.parent / 'data' / 'dictsource' / 'de_DE_roots.json'
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(list(roots), f, ensure_ascii=False, indent=2)
        
    print(f"Saved roots to {out_path}")

if __name__ == "__main__":
    build_root_dictionary()
