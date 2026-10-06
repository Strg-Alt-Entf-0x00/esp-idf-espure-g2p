import json
import urllib.request
import sys
import pathlib
import os

KAIKKI_URL = "https://kaikki.org/dictionary/German/kaikki.org-dictionary-German.jsonl"
OUT_FILE = pathlib.Path(__file__).parent.parent / "test" / "ground_truth" / "de_DE_ground_truth.json"

def fetch_and_parse():
    print(f"Downloading Kaikki.org German Dictionary...")
    print(f"URL: {KAIKKI_URL}")
    print("This file is large (~150MB). Please wait...")
    
    req = urllib.request.Request(KAIKKI_URL, headers={'User-Agent': 'Mozilla/5.0'})
    
    ipa_dict = {}
    word_count = 0
    ipa_count = 0
    
    try:
        with urllib.request.urlopen(req) as response:
            for line_bytes in response:
                word_count += 1
                if word_count % 100000 == 0:
                    print(f"Processed {word_count} entries... found {ipa_count} words with IPA.")
                    
                line = line_bytes.decode('utf-8').strip()
                if not line:
                    continue
                
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue
                
                word = data.get("word")
                if not word:
                    continue
                    
                # We only want basic words, no multi-word phrases
                if " " in word or "-" in word:
                    continue
                
                sounds = data.get("sounds", [])
                ipas = []
                for sound in sounds:
                    if "ipa" in sound:
                        val = sound["ipa"]
                        if val not in ipas:
                            ipas.append(val)
                
                if ipas:
                    ipa_dict[word] = ipas[0] if len(ipas) == 1 else ipas
                    ipa_count += 1

    except Exception as e:
        print(f"Error fetching data: {e}")
        return

    print(f"\nFinished parsing {word_count} total entries.")
    print(f"Extracted {ipa_count} unique German words with IPA pronunciation!")
    
    OUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_FILE, "w", encoding="utf-8") as f:
        json.dump(ipa_dict, f, ensure_ascii=False, indent=2)
        
    print(f"Successfully saved Ground Truth to: {OUT_FILE}")

if __name__ == "__main__":
    fetch_and_parse()

