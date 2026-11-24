# Changelog

This log records notable, verified changes from 2026-10-24 onward. Earlier
history has not been reconstructed.

## 2026-10-27

- Corrected the README's description of the local 698,041-entry pronunciation
  dataset: it was generated from open-dict-data/ipa-dict and is not verified
  ground truth; prior broad-normalization scores do not establish accuracy.
- Documented the host C test workflow and replaced the unverified accuracy
  claim in the component manifest with a neutral project description.
- Fixed the host-test target naming collision that had caused the memory test
  to link against the shared library instead of the instrumented static runtime.
- Added Windows and Linux CI coverage for both German dictionary representations.
- Fixed compressed dictionary routing so enabling German Huffman data does not
  redirect the `en-GB` pilot's lookups to the German dictionary.

## 2026-10-28

- Corrected both ESP32-P4 example PSRAM defaults to the supported 16-line HEX
  mode, removed the redundant hidden UART baud override, and aligned their
  hardware descriptions with ESP-IDF 6.0.2.
