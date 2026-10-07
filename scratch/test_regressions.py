import json
import io
import sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
from espyak.api import G2P
from morph_analyzer import GermanMorphAnalyzer

def normalize_ipa(ipa_str):
    ipa = ipa_str.replace('/', '').replace('[', '').replace(']', '')
    replacements = {
        'ɡ':'g', 'ʁ':'r', 'ʀ':'r', 'ɐ':'r', 'ɑ':'a', 'ɛ':'e', 'ɔ':'o', 
        'ʏ':'y', 'ʊ':'u', 'ɪ':'i', 'œ':'ø', 'c':'k', 'ç':'x', 
        'ɾ':'r', 'ɜ':'r', '̩':''
    }
    for k, v in replacements.items(): ipa = ipa.replace(k, v)
    ipa = ipa.replace('n', 'ən').replace('əən', 'ən')
    for c in ['ˈ', 'ˌ', '.', 'ː', 'ˑ', '̯', '͡', '̥', 'ʰ', '(', ')', '‿', ' ', '-']: ipa = ipa.replace(c, '')
    return ipa.strip()

g2p_rules = G2P('de')
g2p_rules._tr.dict.words = {}
g2p_rules._tr.dict.cased_keys = {}

analyzer = GermanMorphAnalyzer()
gt_path = 'test/ground_truth/de_DE_ground_truth.json'
gt = json.load(open(gt_path, 'r', encoding='utf-8'))

words_to_check = ["hinauf", "hausaufgabe", "apfelbaum", "wunderbar", "beispiel"]

for w in list(gt.keys())[1000:2000]:
    if len(w) > 6:
        words_to_check.append(w)
        if len(words_to_check) > 20: break

print("Analyzing Morphological Regressions...")
for w in words_to_check:
    expected_ipas = gt.get(w, [])
    if isinstance(expected_ipas, str): expected_ipas = [expected_ipas]
    expected_norm = [normalize_ipa(e) for e in expected_ipas]
    
    morphed = analyzer.split_word(w)
    
    if morphed == w: continue
    
    actual_rules = normalize_ipa(g2p_rules.phonemize(w, ipa=True))
    actual_morphed = normalize_ipa(g2p_rules.phonemize(morphed, ipa=True))
    
    if actual_rules in expected_norm and actual_morphed not in expected_norm:
        print(f"REGRESSION: {w}")
        print(f"  Split: {morphed}")
        print(f"  GT:    {expected_norm}")
        print(f"  Raw:   {actual_rules}")
        print(f"  Morph: {actual_morphed}")
        print("-" * 40)
