import sys
sys.path.append('tools')
from morph_analyzer import GermanMorphAnalyzer

analyzer = GermanMorphAnalyzer()
word = 'abarbeiteten'
lower_word = word.lower()
n = len(lower_word)

dp = {n: (0, [])}

for i in range(n - 1, -1, -1):
    best_score = -float('inf')
    best_splits = None
    
    for j in range(i + 1, n + 1):
        root = lower_word[i:j]
        root_len = j - i
        
        is_valid = False
        if root_len >= 3:
            is_valid = (root in analyzer.valid_roots) or (j == n and root in analyzer.terminal_suffixes)
        elif root_len == 2:
            is_valid = (root in analyzer.prefixes) or (root in analyzer.suffixes) or (j == n and root in analyzer.terminal_suffixes) or (root in analyzer.short_roots)
        elif root_len == 1:
            is_valid = (j == n and root in analyzer.terminal_suffixes)
            
        if is_valid:
            if j == n:
                score = (j - i) ** 2
                if score > best_score:
                    best_score = score
                    best_splits = [(i, j, "")]
            else:
                for link in analyzer.linking_elements:
                    link_len = len(link)
                    if j + link_len <= n and lower_word.startswith(link, j):
                        next_i = j + link_len
                        if next_i in dp and dp[next_i][0] != -float('inf'):
                            score = (j - i) ** 2 - 10 + dp[next_i][0]
                            if score > best_score:
                                best_score = score
                                best_splits = [(i, j, link)] + dp[next_i][1]
    dp[i] = (best_score, best_splits)

print(f"Final DP[0]: {dp.get(0)}")
