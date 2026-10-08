import json
import pathlib
from espyak.api import G2P
from morph_analyzer import GermanMorphAnalyzer
from final_benchmark import normalize_ipa

GT_FILE = pathlib.Path(__file__).parent.parent / "data" / "ground_truth" / "de_DE_ground_truth.json"

def analyze_failures():
    with open(GT_FILE, 'r', encoding='utf-8') as f:
        ground_truth = json.load(f)
        
    g2p_rules = G2P('de')
    g2p_rules._tr.dict.words = {} 
    analyzer = GermanMorphAnalyzer()
    
    count = 0
    
    for word, expected_ipas in ground_truth.items():
        if isinstance(expected_ipas, str):
            expected_ipas = [expected_ipas]
            
        expected_norm = [normalize_ipa(ipa) for ipa in expected_ipas]
        
        actual_rules = normalize_ipa(g2p_rules.phonemize(word, ipa=True))
        morphed = analyzer.split_word(word)
        actual_morphed = normalize_ipa(g2p_rules.phonemize(morphed, ipa=True))
        
        rules_ok = actual_rules in expected_norm
        morph_ok = actual_morphed in expected_norm
        
        if not morph_ok:
            print(f"FAILED: {word}")
            print(f"  Split     : {morphed}")
            print(f"  Expected  : {expected_norm[0]}")
            print(f"  Morph Got : {actual_morphed}")
            print("-" * 40)
            count += 1
            if count >= 30:
                break

if __name__ == "__main__":
    analyze_failures()
