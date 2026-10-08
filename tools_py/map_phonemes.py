import json
import pathlib

# Load exceptions
exc_path = pathlib.Path(__file__).parent.parent / "data" / "dictsource" / "de_DE_exceptions.json"
exceptions = json.loads(exc_path.read_text('utf-8'))

# IPA to Espeak mapping for German
mapping = {
    # Consonants
    'p': 'p', 'b': 'b', 't': 't', 'd': 'd', 'k': 'k', 'g': 'g', 'g': 'g',
    'f': 'f', 'v': 'v', 's': 's', 'z': 'z', '?': 'S', '?': 'Z',
    'ç': 'C', 'x': 'x', '?': 'x', 'h': 'h',
    'm': 'm', 'n': 'n', '?': 'N', 'l': 'l', 'j': 'j',
    '?': 'r', '?': 'r', 'r': 'r', '?': '6', # vocalized r is often '6' in espeak or 'r'
    '?': '_', # glottal stop

    # Vowels (Short)
    'a': 'a', '?': 'E', '?': 'I', '?': 'O', '?': 'U', '?': 'Y', 'œ': '9',
    
    # Vowels (Long) - Wiktionary usually adds ?
    'a?': 'a:', 'e?': 'e:', 'i?': 'i:', 'o?': 'o:', 'u?': 'u:', 'y?': 'y:', 'ø?': '2:',
    
    # Schwa
    '?': '@',
    
    # Diphthongs
    'a??': 'aI', 'a?': 'aI',
    'a??': 'aU', 'a?': 'aU',
    '???': 'OY', '??': 'OY',
    
    # Stress markers
    ''': "'",
    '?': ",",
    
    # Affricates (ts, pf, t?, d? are usually just sequences in espeak)
    't?s': 'ts', 'p?f': 'pf', 't??': 'tS', 'd??': 'dZ',
    
    # Remove markers
    '.': '', '?': '', ' ': '_', '?': ':'
}

def translate_ipa_to_espeak(ipa):
    # Diphthongs and Affricates first
    for k in sorted(mapping.keys(), key=len, reverse=True):
        if k in ipa:
            ipa = ipa.replace(k, mapping[k])
    
    # Any remaining ? mapped to :
    ipa = ipa.replace('?', ':')
    
    # Remove any unmapped unicode (clean up)
    allowed = set("pbtdkgfvszSZCxhmnNljr6_aEIOUY9e:i:o:u:y:2:@',:")
    res = ""
    for c in ipa:
        if c in allowed:
            res += c
    return res

mapped_dict = {}
for w, ipa in exceptions.items():
    esp_phonemes = translate_ipa_to_espeak(ipa)
    if esp_phonemes:
        mapped_dict[w] = esp_phonemes

out_path = pathlib.Path(__file__).parent.parent / "data" / "dictsource" / "de_DE_espeak_exceptions.json"
out_path.write_text(json.dumps(mapped_dict, ensure_ascii=False, indent=2), 'utf-8')
print(f"Mapped {len(mapped_dict)} words to espeak phonemes.")

