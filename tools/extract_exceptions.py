import sys
import pathlib
import io
import json
import time
import unicodedata

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

from espyak.api import G2P
from espyak.rule_compiler import RuleSet

def normalize_ipa(ipa_str):
    ipa = ipa_str.replace('/', '').replace('[', '').replace(']', '')
    
    # Map narrow phonetic symbols to broad phonemes for fair comparison
    replacements = {
        'ɡ': 'g',  # U+0261 -> U+0067
        'ʁ': 'r',
        'ʀ': 'r',
        'ɐ': 'r',
        'ɑ': 'a',
        'ɛ': 'e',
        'ɔ': 'o',
        'ʏ': 'y',
        'ʊ': 'u',
        'ɪ': 'i',
        'œ': 'ø',
        'c': 'k',  # Sometimes 'ch' is c or ç, but let's stick to core matches
        'ç': 'x',  # Ich-laut vs Ach-laut normalization
    }
    for k, v in replacements.items():
        ipa = ipa.replace(k, v)
        
    # Remove prosody, stress, length, and tie markers
    remove_chars = ['ˈ', 'ˌ', '.', 'ː', 'ˑ', '̯', '͡', '̥', 'ʰ', '(', ')', '‿', ' ']
    for c in remove_chars:
        ipa = ipa.replace(c, '')
        
    return ipa.strip()

print("Loading Ground Truth from Kaikki.org...")
gt_path = pathlib.Path(__file__).parent.parent / 'test' / 'ground_truth' / 'de_DE_ground_truth.json'
with open(gt_path, 'r', encoding='utf-8') as f:
    ground_truth = json.load(f)

print("Initializing Rules Engine...")
g2p_rules_only = G2P('de')

rule_path = str(pathlib.Path(__file__).parent.parent / 'data' / 'dictsource' / 'de_DE_rules')
rules = RuleSet.compile_file(rule_path)
g2p_rules_only._tr.rules = rules

# Disable dictionary completely, pure rules!
g2p_rules_only._tr.dict.words = {}
g2p_rules_only._tr.dict.cased_keys = {}
g2p_rules_only._tr.dict._raw_keys = []

words = list(ground_truth.keys())
total_words = len(words)
print(f"Loaded {total_words} words for scientific evaluation...")

fail_count = 0
match_count = 0

log_path = str(pathlib.Path(__file__).parent.parent / 'regression_mismatches.log')
with open(log_path, "w", encoding="utf-8") as f_log:
    f_log.write("WORD | EXPECTED (Normalized Wiktionary) | ACTUAL (Normalized Rules)\n")
    f_log.write("-" * 80 + "\n")
    
    start_time = time.time()
    
    for idx, word in enumerate(words):
        if len(word) < 2:
            continue
            
        expected_ipas = ground_truth[word]
        if isinstance(expected_ipas, str):
            expected_ipas = [expected_ipas]
            
        # Normalize all Wiktionary IPAs
        expected_norm = [normalize_ipa(e) for e in expected_ipas]
        
        # Phonemize and Normalize our engine
        actual_raw = g2p_rules_only.phonemize(word, ipa=True)
        actual_norm = normalize_ipa(actual_raw)
        
        # Check against broad representation
        if actual_norm in expected_norm:
            match_count += 1
        else:
            fail_count += 1
            expected_str = ' OR '.join(expected_norm)
            f_log.write(f"{word} | {expected_str} | {actual_norm} (Raw: {actual_raw.strip()})\n")
            
        if idx > 0 and idx % 10000 == 0:
            print(f"Processed {idx}/{total_words} words... (Current Broad Accuracy: {match_count/idx*100:.2f}%)")

elapsed = time.time() - start_time
accuracy = (match_count / (match_count + fail_count)) * 100

print("\n" + "="*50)
print("SCIENTIFIC EVALUATION COMPLETE")
print("="*50)
print(f"Total Words Tested: {match_count + fail_count}")
print(f"Perfect Broad Matches: {match_count}")
print(f"Mismatches:            {fail_count}")
print(f"Broad Accuracy:        {accuracy:.3f}%")
print(f"Time Taken:            {elapsed:.1f} seconds")
print("="*50)


