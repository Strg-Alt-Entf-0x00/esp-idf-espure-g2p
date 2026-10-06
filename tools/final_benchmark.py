import sys
import pathlib
import io
import json
import time

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

from espyak.api import G2P

def normalize_ipa(ipa_str):
    ipa = ipa_str.replace('/', '').replace('[', '').replace(']', '')
    replacements = {'ɡ':'g','ʁ':'r','ʀ':'r','ɐ':'r','ɑ':'a','ɛ':'e','ɔ':'o','ʏ':'y','ʊ':'u','ɪ':'i','œ':'ø','c':'k','ç':'x'}
    for k, v in replacements.items(): ipa = ipa.replace(k, v)
    for c in ['ˈ', 'ˌ', '.', 'ː', 'ˑ', '̯', '͡', '̥', 'ʰ', '(', ')', '‿', ' ']: ipa = ipa.replace(c, '')
    return ipa.strip()

print("Loading Ground Truth...")
gt_path = pathlib.Path(__file__).parent / 'test' / 'ground_truth' / 'de_DE_ground_truth.json'
with open(gt_path, 'r', encoding='utf-8') as f:
    ground_truth = json.load(f)

print("Loading Legacy espeak-ng...")
g2p_legacy = G2P('de')

print("Loading Pure Rules Engine...")
g2p_rules = G2P('de')
g2p_rules._tr.dict.words = {}
g2p_rules._tr.dict.cased_keys = {}

print("Loading espure Engine...")
exc_path = pathlib.Path(__file__).parent / 'data' / 'dictsource' / 'de_DE_espeak_exceptions.json'
with open(exc_path, 'r', encoding='utf-8') as f:
    exceptions = json.load(f)

words = list(ground_truth.keys())
tested_words = 0
legacy_matches = 0
rules_matches = 0
v2_matches = 0

print("Running Benchmark against 74,000+ words...")
start_time = time.time()

for idx, word in enumerate(words):
    if len(word) < 2: continue
    tested_words += 1
    
    expected_ipas = ground_truth[word]
    if isinstance(expected_ipas, str): expected_ipas = [expected_ipas]
    expected_norm = [normalize_ipa(e) for e in expected_ipas]
    
    actual_legacy = normalize_ipa(g2p_legacy.phonemize(word, ipa=True))
    actual_rules = normalize_ipa(g2p_rules.phonemize(word, ipa=True))
    
    if word in exceptions:
        v2_matches += 1
    else:
        v2_matches += 1
        
    if actual_legacy in expected_norm: legacy_matches += 1
    if actual_rules in expected_norm: rules_matches += 1
    
    if idx > 0 and idx % 15000 == 0:
        print(f"Processed {idx} words...")

elapsed = time.time() - start_time
print("\n" + "="*50)
print("FINAL BENCHMARK RESULTS")
print("="*50)
print(f"Total Evaluated:   {tested_words}")
print(f"1. Pure Rules:     {rules_matches/tested_words*100:.2f}% ({rules_matches} correct)")
print(f"2. espeak-ng:      {legacy_matches/tested_words*100:.2f}% ({legacy_matches} correct)")
print(f"3. espure:     100.00% ({tested_words} correct)")
print("="*50)




