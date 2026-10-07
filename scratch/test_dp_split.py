import json
import pathlib
import sys

class GermanMorphAnalyzerDP:
    def __init__(self):
        gt_path = pathlib.Path('data') / 'dictsource' / 'de_DE_roots.json'
        with open(gt_path, 'r', encoding='utf-8') as f:
            gt = json.load(f)
            
        self.valid_roots = set(w.lower() for w in gt if len(w) >= 3)
        
        self.prefixes = {"ab", "an", "auf", "aus", "be", "bei", "da", "dar", "durch", "ein", "emp", 
                         "ent", "er", "fort", "ge", "her", "hin", "hinter", "mit", "nach", "nieder", 
                         "ober", "unter", "über", "um", "un", "ur", "ver", "vor", "weg", "wider", 
                         "wieder", "zer", "zu", "zurecht", "zurück", "zusammen", "zwischen",
                         "bundes", "kinder", "haupt", "sonder", "super", "halb", "lieblings"}
                         
        self.suffixes = {"bar", "chen", "ei", "en", "end", "er", "haft", "heit", "ie", "ig", "in", 
                         "isch", "keit", "lein", "lich", "ling", "nis", "sal", "sam", "schaft", 
                         "tum", "ung", "werk", "los", "voll", "mäßig", "innen"}
                         
        self.linking_elements = {"", "s", "es", "n", "en", "e", "er"}

        for p in self.prefixes: self.valid_roots.add(p)
        for s in self.suffixes: self.valid_roots.add(s)

    def split_word(self, word):
        if not word or len(word) < 5: return word
        lower_word = word.lower()
        n = len(lower_word)
        
        dp = {n: (0, [])}
        
        for i in range(n - 1, -1, -1):
            best_score = -float('inf')
            best_splits = None
            
            for j in range(i + 3, n + 1):
                root = lower_word[i:j]
                if root in self.valid_roots:
                    if j == n:
                        score = (j - i) ** 2
                        if score > best_score:
                            best_score = score
                            best_splits = [(i, j, "")]
                    else:
                        for link in self.linking_elements:
                            link_len = len(link)
                            if j + link_len <= n and lower_word.startswith(link, j):
                                next_i = j + link_len
                                if next_i in dp and dp[next_i][0] != -float('inf'):
                                    score = (j - i) ** 2 - 10 + dp[next_i][0]
                                    if score > best_score:
                                        best_score = score
                                        best_splits = [(i, j, link)] + dp[next_i][1]
            dp[i] = (best_score, best_splits)
            
        if 0 in dp and dp[0][0] != -float('inf') and dp[0][1] and len(dp[0][1]) > 1:
            result = ""
            prev_was_end = False
            for start, end, link in dp[0][1]:
                part = word[start:end]
                part_lower = part.lower()
                orig_link = word[end:end+len(link)]
                
                is_prefix = part_lower in self.prefixes
                is_suffix = part_lower in self.suffixes
                is_root = not is_prefix and not is_suffix
                
                if result:
                    if prev_was_end and not is_suffix:
                        result += "-"
                        
                result += part + orig_link
                
                # A part ends a compound word if it is a root, or a suffix
                if is_root or is_suffix:
                    prev_was_end = True
                else:
                    prev_was_end = False
                    
            return result
            
        return word

if __name__ == "__main__":
    analyzer = GermanMorphAnalyzerDP()
    test_words = [
        "hintragen", 
        "Kinderstube", 
        "Bundeskanzlerin", 
        "Donaudampfschiff", 
        "Unfreundlichkeit", 
        "Verantwortung", 
        "Wachtraum",
        "Wachstumsphase",
        "Arbeitslosigkeit",
        "Aufmerksamkeitsdefizit",
        "Abend"
    ]
    for w in test_words:
        print(f"{w:25} -> {analyzer.split_word(w)}")
