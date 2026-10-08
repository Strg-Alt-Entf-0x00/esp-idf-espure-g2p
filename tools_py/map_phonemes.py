import json
import pathlib

# Load exceptions
exc_path = pathlib.Path(__file__).parent.parent / "data" / "dictsource" / "de_DE_exceptions.json"
exceptions = json.loads(exc_path.read_text('utf-8'))

# IPA to Espeak mapping for German
mapping = {
    # Consonants
    'p': 'p', 'b': 'b', 't': 't', 'd': 'd', 'k': 'k', 'ɡ': 'g', 'g': 'g',
    'f': 'f', 'v': 'v', 's': 's', 'z': 'z', 'ʃ': 'S', 'ʒ': 'Z',
    'ç': 'C', 'x': 'x', 'χ': 'x', 'h': 'h',
    'm': 'm', 'n': 'n', 'ŋ': 'N', 'l': 'l', 'j': 'j',
    'ʁ': 'r', 'ʀ': 'r', 'r': 'r', 'ɐ': '6', # vocalized r is often '6' in espeak or 'r'
    'ʔ': '_', # glottal stop

    # Vowels (Short)
    'a': 'a', 'ɛ': 'E', 'ɪ': 'I', 'ɔ': 'O', 'ʊ': 'U', 'ʏ': 'Y', 'œ': '9',
    
    # Vowels (Long) - Wiktionary usually adds ː
    'aː': 'a:', 'eː': 'e:', 'iː': 'i:', 'oː': 'o:', 'uː': 'u:', 'yː': 'y:', 'øː': '2:',
    
    # Schwa
    'ə': '@',
    
    # Diphthongs
    'aɪ̯': 'aI', 'aɪ': 'aI',
    'aʊ̯': 'aU', 'aʊ': 'aU',
    'ɔʏ̯': 'OY', 'ɔʏ': 'OY',
    
    # Stress markers
    'ˈ': "'",
    'ˌ': ",",
    
    # Affricates (ts, pf, tʃ, dʒ are usually just sequences in espeak)
    't͡s': 'ts', 'p͡f': 'pf', 't͡ʃ': 'tS', 'd͡ʒ': 'dZ',
    
    # Remove markers
    '.': '', '̯': '', ' ': '_', 'ː': ':'
}

def translate_ipa_to_espeak(ipa):
    # Diphthongs and Affricates first
    for k in sorted(mapping.keys(), key=len, reverse=True):
        if k in ipa:
            ipa = ipa.replace(k, mapping[k])
    
    # Any remaining ː mapped to :
    ipa = ipa.replace('ː', ':')
    
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

