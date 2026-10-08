import json
import pathlib
from collections import Counter
from espyak.api import G2P
from morph_analyzer import GermanMorphAnalyzer
from final_benchmark import normalize_ipa

GT_FILE = pathlib.Path(__file__).parent.parent / "test" / "ground_truth" / "de_DE_ground_truth_large.json"

def analyze_patterns():
    print("Loading Massive Ground Truth (698k words)...")
    with open(GT_FILE, 'r', encoding='utf-8') as f:
        ground_truth = json.load(f)
        
    g2p_rules = G2P('de')
    g2p_rules._tr.dict.words = {} 
    analyzer = GermanMorphAnalyzer()
    
    # We will just analyze the first 500,000 words to save time
    limit = 500000
    count = 0
    
    endings_counter = Counter()
    
    for word, expected_ipas in ground_truth.items():
        if isinstance(expected_ipas, str):
            expected_ipas = [expected_ipas]
            
        expected_norm = [normalize_ipa(ipa) for ipa in expected_ipas]
        
        morphed = analyzer.split_word(word)
        actual_morphed = normalize_ipa(g2p_rules.phonemize(morphed, ipa=True))
        
        if actual_morphed not in expected_norm:
            # Word failed. Let's record its last 3, 4, and 5 letters to find patterns!
            if len(word) >= 3: endings_counter[word[-3:]] += 1
            if len(word) >= 4: endings_counter[word[-4:]] += 1
            if len(word) >= 5: endings_counter[word[-5:]] += 1
            
        count += 1
        if count >= limit:
            break
            
    print("\nMost common endings in FAILED words:")
    for ending, freq in endings_counter.most_common(30):
        print(f" -{ending}: {freq} failures")

if __name__ == "__main__":
    analyze_patterns()
