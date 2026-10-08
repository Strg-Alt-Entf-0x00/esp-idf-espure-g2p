import json
import pathlib
import time
from espyak.api import G2P
from morph_analyzer import GermanMorphAnalyzer
from final_benchmark import normalize_ipa

GT_FILE = pathlib.Path(__file__).parent.parent / "data" / "ground_truth" / "de_DE_ground_truth.json"
OUT_FILE = pathlib.Path(__file__).parent.parent / "src" / "de_DE_dict_data.c"

def build_exception_dict():
    print("Loading Massive Ground Truth (698k words)...")
    with open(GT_FILE, 'r', encoding='utf-8') as f:
        ground_truth = json.load(f)
        
    g2p_rules = G2P('de')
    g2p_rules._tr.dict.words = {} # Disable dictionary completely
    
    analyzer = GermanMorphAnalyzer()
    
    total = 0
    correct = 0
    exceptions = []
    
    print(f"Running G2P on {len(ground_truth)} words to filter exceptions...")
    start_time = time.time()
    
    for word, expected_ipas in ground_truth.items():
        if isinstance(expected_ipas, str):
            expected_ipas = [expected_ipas]
            
        expected_norm = [normalize_ipa(ipa) for ipa in expected_ipas]
        
        morphed = analyzer.split_word(word)
        actual = normalize_ipa(g2p_rules.phonemize(morphed, ipa=True))
        
        if actual in expected_norm:
            correct += 1
        else:
            # We take the first valid expected IPA as the dictionary entry
            exceptions.append((word.lower(), expected_norm[0]))
            
        total += 1
        
        if total % 100000 == 0:
            elapsed = time.time() - start_time
            print(f"Processed {total} words... found {len(exceptions)} exceptions so far.")
            
    # Sort exceptions alphabetically for binary search
    exceptions.sort(key=lambda x: x[0])
    
    # Deduplicate exceptions (Wiktionary might have capitalization differences resolving to the same lowercase word)
    unique_exceptions = {}
    for w, ipa in exceptions:
        # If there are conflicts, we just keep the first one
        if w not in unique_exceptions:
            unique_exceptions[w] = ipa
            
    print("\n==================================================")
    print(f"Total Evaluated: {total}")
    print(f"Covered by Rules/Morph: {correct} ({(correct/total)*100:.2f}%)")
    print(f"Exceptions for Dict: {len(unique_exceptions)} ({(len(unique_exceptions)/total)*100:.2f}%)")
    print("==================================================")
    
    print("Generating de_DE_dict_data.c ...")
    with open(OUT_FILE, 'w', encoding='utf-8') as f:
        f.write("#include <stddef.h>\n")
        f.write('#include "espure_internal.h"\n\n')
        f.write(f"// Auto-generated exception dictionary from {len(unique_exceptions)} words\n")
        f.write("const espure_dict_entry_t espure_de_dict[] = {\n")
        
        for w, ipa in unique_exceptions.items():
            # escape backslashes and quotes if any
            ipa_escaped = ipa.replace('\\', '\\\\').replace('"', '\\"')
            w_escaped = w.replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'    {{"{w_escaped}", "{ipa_escaped}"}},\n')
            
        f.write("};\n\n")
        f.write("const size_t espure_de_dict_size = sizeof(espure_de_dict) / sizeof(espure_de_dict[0]);\n")
        
    print(f"Done! Exception Dictionary written to {OUT_FILE}")

if __name__ == "__main__":
    build_exception_dict()
