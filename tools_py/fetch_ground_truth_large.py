import json
import urllib.request
import pathlib

URL = "https://github.com/open-dict-data/ipa-dict/raw/master/data/de.txt"
OUT_FILE = pathlib.Path(__file__).parent.parent / "test" / "ground_truth" / "de_DE_ground_truth_large.json"

def fetch_and_parse():
    print(f"Downloading German IPA Dict from {URL} ...")
    req = urllib.request.Request(URL, headers={'User-Agent': 'Mozilla/5.0'})
    
    ipa_dict = {}
    word_count = 0
    ipa_count = 0
    
    try:
        with urllib.request.urlopen(req) as response:
            text = response.read().decode('utf-8')
            lines = text.split('\n')
            
            for line in lines:
                word_count += 1
                if word_count % 100000 == 0:
                    print(f"Processed {word_count} lines... found {ipa_count} valid words.")
                    
                line = line.strip()
                if not line or '\t' not in line:
                    continue
                
                parts = line.split('\t')
                word = parts[0].strip()
                ipas_raw = parts[1].split(', ')
                
                # Filter out multi-word phrases and numbers
                if " " in word or "-" in word:
                    continue
                    
                # Ensure it is purely alphabetical
                if not word.isalpha():
                    continue
                    
                ipas = [ipa.strip('/').strip() for ipa in ipas_raw if ipa.strip()]
                
                if ipas:
                    ipa_dict[word] = ipas[0] if len(ipas) == 1 else ipas
                    ipa_count += 1

    except Exception as e:
        print(f"Error fetching data: {e}")
        return

    print(f"\nFinished parsing {word_count} total lines.")
    print(f"Extracted {ipa_count} unique valid German words with IPA!")
    
    OUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_FILE, "w", encoding="utf-8") as f:
        json.dump(ipa_dict, f, ensure_ascii=False, indent=2)
        
    print(f"Successfully saved Ground Truth to: {OUT_FILE}")

if __name__ == "__main__":
    fetch_and_parse()
