import json
import time
import os
import pathlib
import sys
from espyak.api import G2P

from final_benchmark import normalize_ipa
from morph_analyzer import GermanMorphAnalyzer

GT_FILE = pathlib.Path(__file__).parent.parent / "test" / "ground_truth" / "de_DE_ground_truth_large.json"

def run_stress_test():
    print("Loading Massive Ground Truth (698k words)...")
    with open(GT_FILE, 'r', encoding='utf-8') as f:
        ground_truth = json.load(f)
        
    print("Loading Pure Rules Engine...")
    g2p_rules = G2P('de')
    g2p_rules._tr.dict.words = {} # Disable dictionary completely

    print("Initializing Morphological Analyzer...")
    analyzer = GermanMorphAnalyzer()
    
    # Optional limit for faster testing, or set to None for full run
    limit = 50000
    
    total = 0
    morphed_matches = 0
    rules_matches = 0
    
    print(f"Running stress test against {len(ground_truth)} words...")
    start_time = time.time()
    
    # We won't test legacy espeak-ng here because it would take forever to load/run it 700k times
    # We just want to see how our Morphological Rules Engine performs vs Pure Rules!
    
    for word, expected_ipas in ground_truth.items():
        if isinstance(expected_ipas, str):
            expected_ipas = [expected_ipas]
            
        expected_norm = [normalize_ipa(ipa) for ipa in expected_ipas]
        
        # 1. Pure Rules Engine Baseline
        actual_rules = normalize_ipa(g2p_rules.phonemize(word, ipa=True))
        
        # 2. Morphological Engine
        morphed = analyzer.split_word(word)
        actual_morphed = normalize_ipa(g2p_rules.phonemize(morphed, ipa=True))
        
        if actual_rules in expected_norm: rules_matches += 1
        if actual_morphed in expected_norm: morphed_matches += 1
        
        total += 1
        
        if total % 50000 == 0:
            elapsed = time.time() - start_time
            print(f"Processed {total} words... ({total/elapsed:.2f} words/sec)")
            
        if limit and total >= limit:
            break
            
    print("\n==================================================")
    print("FINAL STRESS TEST RESULTS")
    print("==================================================")
    print(f"Total Evaluated:   {total}")
    print(f"1. Pure Rules Engine (Baseline):  {rules_matches / total * 100:.2f}% ({rules_matches} correct)")
    print(f"2. Morphological Rules Engine:    {morphed_matches / total * 100:.2f}% ({morphed_matches} correct)")
    print("==================================================")

if __name__ == "__main__":
    run_stress_test()
