# ESPure G2P

ESPure is a native ESP-IDF component for grapheme-to-phoneme (G2P) conversion
on embedded systems. Its initial pronunciation target is Modern Standard
German as spoken in Germany (`de-DE`). The project also contains a small
British English (`en-GB`) architecture pilot.

ESPure handles linguistic analysis and pronunciation output; it does not
synthesize audio. TTS engines such as
[ESPormant32](https://github.com/Strg-Alt-Entf-0x00/esp32-p4-idf-espormant32)
consume its output and own acoustic realization and audio generation.

The project is under active development. Supported behavior, resource usage,
accuracy, language coverage, and compatibility depend on the source revision,
build configuration, and validation performed. Aspirational goals and
exploratory measurements below are not release-level guarantees.

## Project Goals and Research Approach

- **Native embedded implementation:** A clean-room C runtime and public API,
  designed from first principles for ESP32-class targets without an embedded
  JavaScript or bytecode engine. The original project vision also described a
  permissive, unencumbered release; that remains subject to the separate
  licensing and data-provenance review below.
- **Linguistic target:** Standard German (`de-DE`) pronunciation mapped to
  the International Phonetic Alphabet (IPA), with documented standard
  variants and explicit locale ownership.
- **Separation from synthesis:** ESPure provides text-to-pronunciation
  analysis. TTS systems own model-specific tokenization, synthesis planning,
  timing, voice characteristics, acoustic realization, and audio generation.
- **Evaluation direction:** Historical engineering targets included
  sub-microsecond deterministic runtime behavior, no dynamic heap allocation
  in hot paths, a flash footprint below 1 MB (target below 350 KB), and
  evaluation throughput above 40,000 words per second. The project has also
  described evaluation against a curated 698,041-entry ground-truth corpus
  and accuracy goals approaching 99.99%. These are goals or exploratory
  figures, not validated guarantees for the current component. In particular,
  the current benchmark script is not a compiled-runtime benchmark and is
  not pronunciation-accuracy evidence.
- **Data and licensing care:** Runtime architecture, language data, generated
  outputs, and their provenance must be reviewed separately before making
  release or licensing claims. Clean-room implementation intent does not
  establish the licensing status of imported or derived data.

## Implemented Component

- Native C runtime and public API in `src/` and `include/`.
- German (`de-DE`) rules, morphology, phoneme metadata, and lexical data in
  `lang/de_DE/`.
- A four-entry British English (`en-GB`) architecture pilot with independent
  phoneme inventory, IPA map, and generated dictionary. It exercises language
  isolation; it is not release-level English support and has no English rules
  or morphology.
- Build-time locale selection and dictionary representation through Kconfig.
- ESP-IDF console examples and a host smoke-test project.

## Component Boundaries and Output

ESPure's responsibility is text-to-pronunciation analysis for a selected
locale/profile, including linguistic choices and pronunciation features.
Consumer adapters translate that result to their own token IDs without
adding language-specific pronunciation repairs. A TTS frontend or engine
owns model-specific tokenization, sentence-level synthesis planning, timing,
voice characteristics, acoustic realization, and audio generation.

For machine consumers, the intended canonical interface is a versioned,
structured pronunciation result. It should identify the selected
locale/profile and represent ordered ESPure-defined pronunciation segments
with stable symbol identifiers and linguistic features such as stress,
syllable/word boundaries, and segment length where applicable. These
identifiers and their meanings belong to ESPure, not to a consumer's internal
phoneme IDs.

The segments describe a broad phonetic target: ESPure resolves
language-dependent pronunciation choices such as standard allophones, but
does not specify narrow acoustic detail. Contextual coarticulation and its
acoustic realization remain with the TTS system. IPA is a derived,
human-readable and evaluation rendering; Kirshenbaum is a compatibility
rendering.

The existing string APIs and format selectors are the current interface;
they are not yet the proposed versioned structured contract. That contract
requires a separately reviewed API migration and tests. This design
direction does not claim that the structured API already exists.

### Pronunciation target

The initial German target is **Standard German as spoken in Germany
(`de-DE`), in a neutral, careful, supralocal read-aloud style**: clear and
natural for general informational speech (comparable to neutral broadcast
delivery), without regional dialect coloring or theatrical stage diction.
This describes a pronunciation target, not an individual speaker or voice.
Accepted standard variants must be explicitly documented and tested; they
must not be introduced as undocumented rule exceptions. `de-AT`, `de-CH`,
and dialect targets are separate profiles, not implicit alternatives within
`de-DE`.

This is a project-level decision and should not be reopened for each consumer
or TTS integration. Change it only through an explicit, reviewed update to
this section and its corresponding reference tests.

### Locale-owned language data

Public runtime identifiers follow BCP 47: German (Germany) is `de-DE`, while
the British English pilot is `en-GB`. Bare `de` and `en` are intentionally
not accepted because they do not identify a specific regional variety.
Kconfig symbols use underscores because identifiers cannot contain hyphens.
Internal source directories and generated C symbols also retain underscores
where required by the generated-code naming scheme; these are not locale
strings accepted by the runtime API.

Locale IDs identify the target pronunciation model, not each individual
phoneme. Phoneme inventories can be shared at the language level when they
are the same; locale-specific rules, mappings, or overlays belong to a
locale when pronunciations differ. For example, standard German targets may
include `de-DE`, `de-AT`, and `de-CH`. A dialect with its own registered
language tag should use that tag rather than an invented regional suffix.

The EN pilot currently needs only a small exception dictionary and smoke
vectors in `data/`; its generated dictionary, IPA map, and phoneme inventory
live under `lang/en_GB/`. It has no rules or morphology, so there are no
English rule, root, or morphology files. German's corresponding runtime
phoneme inventory and IPA map are language-owned under `lang/de_DE/`; the
old mixed phoneme fixture under `src/` is only for host parser tests.

## Data Pipeline and Provenance

Host-side workflows for rule sources, morphological data, corpora, and
generated dictionary representations are still being reviewed for
deterministic outputs, provenance, capitalization collisions, and
reproducible validation. The evolving development workspace is under
`tools_py/ai_developer/`; its current contents are not a stable public tooling
interface or a proven path to 100% accuracy or one-step language support.

### German runtime data provenance

- The German rule input is `data/dictsource/de_DE_rules`. Its header carries
  eSpeak's GPL-3.0-or-later copyright and license notice. Compiling the rules
  into a C representation changes their format, not their provenance. The
  repository root currently has no `LICENSE*` file, and `idf_component.yml`
  has no license field; project-wide licensing and compatibility obligations
  still need an explicit review.
- `lang/de_DE/de_DE_dict.c` is generated from
  `data/dictsource/de_DE_espeak_exceptions.json`. The filename identifies it
  as eSpeak-derived, but the active JSON does not record per-entry source,
  selection rationale, or review status. The dictionary must not be treated
  as independently verified lexical ground truth.
- `lang/de_DE/de_DE_phonemes_data.c` is a provisional runtime inventory
  assembled to cover current rule and dictionary outputs. Its category
  assignments have not been independently reviewed as a German phonological
  analysis, and it has no checked-in canonical source or generator.
- `lang/de_DE/de_DE_ipa.c` is a language-owned mapping used by the runtime,
  but it likewise has no checked-in canonical source, per-entry provenance,
  or independent linguistic review. Mapping coverage proves renderability,
  not that an IPA value is the correct realization.

Host audits check that active dictionary and compiled-rule tokens are present
in the runtime inventory and can be rendered, that inventory declarations
are unique, and that declared items have a language-owned mapping or an
explicit control-token role. These are consistency checks only; they do not
validate pronunciation correctness, licensing obligations, or the
historical origin of data whose provenance is not recorded.

### Current pipeline status

The repository contains scripts for converting eSpeak-derived rule sources,
processing German lexical data, and generating C dictionary representations.
These stages and their outputs are under review. In particular, normalized
dictionary keys can carry different pronunciations for different
capitalizations. The binary dictionary preserves those distinct spellings
and uses exact-case lookup before a lowercase fallback. Conflicting
capitalization variants without a lowercase entry are not aliased to an
arbitrary pronunciation. Their linguistic validity still requires review;
the generated dictionary must not be assumed to contain only true, reviewed
lexical exceptions.

The small British English pilot dictionary is generated separately from its
own source data and smoke vectors; it is not part of the German dataset.

Existing host-side comparison results are exploratory, not a benchmark of
the compiled ESPure runtime or evidence of pronunciation accuracy. A
reproducible, token-aware German evaluation with held-out data and separately
measured rule and exception coverage remains planned.

Current host-side data transformations and their review status are described
in [Development Tools](documentation/development_tools/DEVELOPMENT_TOOLS.md).

## ESP-IDF Examples

The ESP32-P4 console projects are the current interactive firmware examples:

- [`console_espure`](examples/console_espure/README.md) is the
  user-facing console, with its own ESPure command set on UART0.
- [`console_espure_dev`](examples/console_espure_dev/README.md) has a
  separate developer command implementation and adds UART Filebridge and
  SD-card access on UART1.
- Only the UTF-8 line editor is shared, from
  `examples/common/uart_utf8_console/`; command logic is independent.

Both examples currently enable German and the limited `en-GB` pilot and use
the compressed German dictionary setting. Their detailed build, board, and
serial setup is in each example README.

For general ESP-IDF dependency setup, locale configuration, and API usage,
see the [integration guide](documentation/INTEGRATION.md).

## Documentation

- [Integration and quick start](documentation/INTEGRATION.md)
- [Development tools and AI-developer direction](documentation/development_tools/DEVELOPMENT_TOOLS.md)
- [User console example](examples/console_espure/README.md)
- [Developer console example](examples/console_espure_dev/README.md)

The existing [Filebridge guide](documentation/development_tools/FILEBRIDGE.md)
still contains ESPormant32-specific workflow details and is not an
authoritative ESPure guide. For the ESPure developer example's current board
and Filebridge configuration, use its
[README](examples/console_espure_dev/README.md).
