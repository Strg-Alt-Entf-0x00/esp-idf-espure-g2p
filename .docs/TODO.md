# Future Vision & Roadmap

## 1. Clean-Room Rewrite of eSpeak Rules (Long Term)
*Current State:* The core G2P phonetic rules are compiled from legacy `eSpeak-ng` rule files (e.g. `de_DE_rules`). These files use an ancient, complex, and highly fragile syntax filled with obscure exception rules and workarounds (`P2`, `_!'ap`, etc.). Because of this fragility, edge cases (like glottal stops at prefix boundaries, e.g. *abarbeiteten*) cannot be safely fixed in the rule-set without risking massive regressions elsewhere. We currently solve this safely and elegantly via the 9MB Flash Dictionary.

*Future Vision:* To truly make this project "the best in the world" from the ground up, we could replace the legacy eSpeak rule compilation entirely, **without having to rewrite the rules from scratch**. 
We would build a smart Python Parser that reads the old `de_rules`, strips away the cryptic 20-year-old eSpeak syntax, and translates the phonetics into a modern, readable JSON format (e.g., `{"prefix": "ab", "next": "consonant", "phonemes": ["a", "p"]}`).
From this JSON, we would then compile our C-structs.

**Goal:** 
1. **Readable Syntax:** Rules defined in clean JSON. Completely decoupled from the espeak-ng repository.
2. **Modular Architecture:** Strict separation between Vowel Rules, Consonant Rules, and Compound Boundary Rules.
3. **Glottal Stop Awareness:** Native understanding of German morpho-phonology.

**Risk:** Writing the intelligent Python parser to perfectly translate the old syntax into JSON without losing edge cases is a complex task. Until this is undertaken, the current Python compiler (`export_rules_to_c.py`) + C-Morph-Splitter approach is the most professional, robust, and mathematically proven solution.
