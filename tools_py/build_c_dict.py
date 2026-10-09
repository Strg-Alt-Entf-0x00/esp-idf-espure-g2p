import json
import pathlib

in_path = pathlib.Path(__file__).parent.parent / "data" / "dictsource" / "de_DE_espeak_exceptions.json"
exceptions = json.loads(in_path.read_text('utf-8'))

out_path = pathlib.Path(__file__).parent.parent / "lang" / "de_DE" / "de_DE_dict.c"

c_code = """#include "espure_internal.h"

/**
 * @brief Exception dictionary for language: de
 * 
 * Auto-generated from Wiktionary Kaikki dump.
 * Contains over 51,000 exceptions to bridge the gap between pure rules and perfect phonetic representation.
 */
const espure_dict_entry_t DICT_DE_DE[] = {
"""

# Sort for binary search compatibility!
# Force lowercase all keys so C strcmp binary search works!
exceptions = {k.lower(): v for k, v in exceptions.items()}
words = sorted(exceptions.keys())

entries = []
for w in words:
    phonemes = exceptions[w]
    # Escape quotes and backslashes just in case
    w_esc = w.replace('"', '\\"')
    p_esc = phonemes.replace('"', '\\"')
    entries.append(f'    {{"{w_esc}", "{p_esc}", 0}}')

c_code += ",\n".join(entries)

c_code += f"""
}};
const size_t DICT_DE_DE_SIZE = {len(words)};
"""

out_path.write_text(c_code, 'utf-8')
print(f"Generated {out_path.name} with {len(words)} entries.")

