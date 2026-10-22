# ESPure Development Tools

## Status and Purpose

This document records the current host-side development-tooling state and
recommends a target architecture for its future AI-assisted development
environment. The target is a design decision to guide an evidence-first
migration; it is not a claim that the target or every described workflow
already exists.

The long-term goal is a dependable development system that helps a developer
and AI agent repeatedly make evidence-backed improvements to ESPure's
grapheme-to-phoneme behavior. It should be useful interactively and
automatable in bounded, inspectable steps. "Autonomous" means that the
system can carry a well-scoped experiment from baseline to tested candidate
and report; it does not mean that it may silently accept linguistic data,
change project policy, or perform destructive device operations.

Keep observed repository facts, implemented capabilities, design
requirements, hypotheses, and open decisions clearly distinguished. A
directory name, old note, generated result, or AI consensus is not evidence
that a workflow or pronunciation is valid.

## Project Boundaries

ESPure owns grapheme-to-pronunciation analysis and its language resources.
It does not own text-to-speech synthesis or audio hardware. A consumer such
as ESPormant32 owns its adapter and synthesis pipeline. Keep changes in the
repository that owns the behavior; report an upstream or consumer-side
dependency issue rather than silently copying or patching another project's
implementation here.

The user and developer console examples are separate firmware applications
with independent command implementations. Their READMEs own their build,
flash, board, and serial procedures. The developer example's Filebridge and
SD-card listing do not provide runtime dictionary replacement: selected
ESPure language data is embedded in firmware at build time, and the current
API does not load dictionaries from the SD card. Its `ls` command lists SD
files; it does not activate or load them into ESPure.

## Current Repository State

### Root-level utility

`tools_py/build_dict.py` is currently the single standalone Python utility
retained directly under `tools_py/`. It generates C dictionary
representations from the locale-specific JSON exception sources. Its
supported invocation is:

```powershell
python tools_py\build_dict.py --locale de-DE --format huffman
```

The script currently supports locales `de-DE` and `en-GB`, and formats
`plain` and `huffman` (the default is `plain`). It reads the corresponding
source under `data/dictsource/` and writes the generated C file to the
locale's configured path under `lang/`. The output path is fixed by the
script and an existing generated file is overwritten. Review the source
changes and generated diff before accepting or committing the result.

This utility is a representation generator, not a tool for authoring or
reviewing pronunciations. Successful generation does not establish lexical
correctness, provenance, licensing compatibility, or runtime behavior. Its
implementation currently uses Python's standard library; no third-party
Python package is required for this command.

### AI-developer workspace

`tools_py/ai_developer/` is an evolving work area containing host-side
scripts, tests, a corpus-mining project, research notes, source data,
downloads, generated reports, scratch files, caches, and host-build output.
It is not currently a unified installable AI-developer package: there is no
workspace-level package manifest, supported common entry point, or coherent
dependency profile. The corpus miner has its own CLI and a broad dependency
set, including heavyweight NLP/model packages. Its README describes intended
behavior, but those descriptions are not themselves evidence that its
sources, quality checks, or outputs are validated.

The current contents include useful candidate code and tests, an archived
corpus-miner area, local corpora/data, external-source download areas,
scratch scripts, caches, and generated host-build content. These have
different retention, licensing, reproducibility, and testability
requirements. Classify them before moving anything. Some documentation
paths are explicitly marked as not to be read or deleted; leave those paths
untouched and out of automated inventory content reads.

The root `tools_py/build_dict.py` is distinct from the development workspace
and is the one currently retained standalone Python utility at that level.
The repository has no installable Python binding under `bindings/python`.
Do not describe archived or exploratory scripts as supported product
features; verify entry points, dependencies, tests, data flow, and behavior.

## Final Goal and Success Criteria

The desired system is an **ESPure G2P engineering loop**, not a general
autonomous coding agent and not a second G2P implementation in Python. It
should help answer questions such as:

- Which pronunciation failures are reproducible, and what linguistic or
  implementation cause is supported by evidence?
- Is the failure in normalization, morphology, rules, lexical data, phoneme
  inventory/rendering, the public API, or consumer integration?
- Does a proposed rule or data change improve the defined evaluation set
  without introducing regressions, locale leakage, or embedded-resource
  problems?
- Can the result be reproduced from its source revision, inputs,
  configuration, and tools?

The end-to-end loop is: inspect and scope; establish a baseline; form a
testable hypothesis; prepare or select traceable inputs; run host tests and
evaluation; make a bounded candidate change; rebuild and compare; optionally
validate on hardware; then produce a report and reviewable patch. A run that
fails, lacks required inputs, or cannot reproduce its baseline must say so
explicitly.

The system is successful when it can produce **reviewable candidate
improvements with reliable evidence**, not when it maximizes the number of
scripts, mined words, autonomous actions, or a single aggregate score.
Neither perfect pronunciation nor a specific accuracy percentage is an
acceptance criterion until a valid target corpus and evaluation method have
been independently established.

### Evidence required for an improvement

Every evaluation protocol should state which of these dimensions it
actually measures; none substitutes for the others:

1. **Linguistic behavior:** exact word match and token-aware phoneme error
   metrics against a documented reference, with locale, pronunciation
   target, normalization, exclusions, and ambiguous variants explicit.
2. **Coverage and failure modes:** rule, morphology, exception-dictionary,
   unknown-word, and error-category counts. Coverage is not correctness.
3. **Runtime/API behavior:** UTF-8 handling, deterministic output, locale
   isolation, buffer/error behavior, and regression cases through the real
   implementation. A Python reimplementation is not a substitute for
   testing the C runtime.
4. **Embedded behavior:** target build, flash/resource size, memory use,
   latency, and hardware behavior where these measurements are needed and
   actually captured. Host timing must not be presented as device timing.
5. **Data quality:** provenance, rights, schema validity, duplicate and
   capitalization collisions, review status, and evaluation leakage risks
   where relevant.

Comparisons must use compatible inputs and fixed methods. Keep source
references, generated candidates, human-reviewed labels, and held-out
evaluation data distinct. External G2P systems can be comparison signals,
not unquestioned ground truth. Agreement between models does not prove
correctness.

## Recommended Final Architecture

Use a **modular monolith with explicit boundaries**: one installable
host-side package and CLI, organized around workflows and real external
interfaces. Do not begin with microservices, a plugin marketplace, a task
queue, a mandatory database server, or an always-running agent. Introduce
those only if a demonstrated workflow requires them.

The AI is an optional planner/operator of deterministic project tools; it is
not the source of truth. The same validation and evaluation workflows should
be runnable without an LLM. Keep model-provider integration behind a narrow
interface and out of the core dependency set. AI output is a candidate with
explicit inputs, tool calls, and validation results.

### Target source layout

This is the recommended end state, not a request to create empty folders
immediately. Add a module only when a real, tested workflow needs it.

```text
tools_py/
  build_dict.py                       # Standalone dictionary builder
  ai_developer/
    README.md                         # Setup, supported commands, safety
    pyproject.toml                    # Package and optional dependencies
    src/
      espure_ai_developer/
        __init__.py
        __main__.py                   # python -m entry point
        cli.py                        # Thin command parsing and display
        domain/                       # Locale, provenance, result contracts
        application/
          workflows/                  # Audited use cases, not script dumps
          policies.py                 # Write/device/autonomy authorization
          agent/                      # Bounded AI loop and allowlisted tools
        adapters/
          repository/                 # Paths, git identity, output checks
          host_engine/                # Real C/host test and eval harness
          idf/                        # ESP-IDF build/test/device integration
          corpus/                     # Local and optional external sources
          references/                 # Optional external G2P systems
          model_providers/            # Optional AI-provider integration
          serial/                     # Console transport
          filebridge/                 # Filebridge protocol integration
          storage/                    # Run manifests and local indexes
        reporting/                    # Human and machine-readable results
        schemas/                      # Versioned machine-readable contracts
    tests/
      unit/                           # Offline, fast, deterministic
      integration/                    # Local tools and adapters
      hardware/                       # Explicit board/port, opt-in only
      fixtures/                       # Small, redistributable test inputs
```

`domain/` should contain small, stable contracts rather than duplicate
language rules or phoneme inventories. `application/workflows/` coordinates
use cases without owning UART, network, database, or subprocess details.
`application/agent/` may plan and sequence those workflows, but it should
invoke only explicitly registered tools with typed inputs, bounded outputs,
and policy checks. `adapters/` contains concrete external boundaries;
`model_providers/` is optional and must not leak provider-specific objects
into the application or domain. `cli.py` remains a thin interface. Prefer a
few coherent modules over one file per noun or a framework-heavy
architecture.

The package manifest should provide a minimal base install and optional
dependency groups for tests, corpus connectors, external reference models,
and hardware/Filebridge work. Do not make Torch, Transformers, web crawlers,
or downloadable language models prerequisites for dictionary generation,
offline tests, or ordinary CLI startup. Optional integrations should fail
with a precise install hint when invoked, not break unrelated workflows.
Select a Python-version floor and package manager only after auditing actual
source and ESP-IDF environment requirements; document and test the chosen
support matrix.

### Keep data and run state outside the package

The Python package should contain code, schemas, and small test fixtures—not
working corpora, downloads, model weights, caches, build trees, or experiment
outputs. Keep canonical runtime inputs and generated runtime source in their
existing project-owned locations (`data/`, `lang/`, and related ESP-IDF
sources). Do not silently create a second authority under
`tools_py/ai_developer/data/`.

Use a configurable local workspace outside the repository checkout for
downloads, caches, private configuration, and run outputs. Support a
platform-appropriate default and an explicit workspace/configuration
override. On Windows this may live under the user's local application-data
directory; paths must not be hard-coded to one machine or COM assignment.
Only small, legally redistributable, deterministic fixtures belong in Git.

Each run should produce an immutable run directory with a versioned
`manifest.json`, raw logs/outputs, derived results, and a human-readable
summary. The manifest should identify, where applicable:

- Run ID, UTC start/end, completion state, and schema version.
- Repository revision and whether the checkout had uncommitted changes.
- Locale, target pronunciation profile, Kconfig/build settings, and device
  identity when used.
- Input dataset IDs, source/license/review status, selected sample IDs, and
  hashes where practical.
- Exact command/workflow, tool versions, normalization and metric versions,
  and relevant configuration fingerprints.
- Artifacts, their roles, checksums, transformations, and links to raw
  evidence.
- Failures, skipped stages, missing inputs, and operator approvals.

Do not store credentials, access tokens, or unnecessary personal information
in manifests or logs. A report must never hide a failed or incomplete stage.

### Workflow boundaries

- **Repository and baseline:** identify the actual checkout, target, tool
  availability, relevant tests, and initial source/data state.
- **Data audit and preparation:** inspect schemas, provenance, duplicates,
  locale/case collisions, encoding, and rights. Produce candidates and
  reports; do not automatically call mined or model-generated pronunciations
  verified ground truth.
- **Dictionary generation:** keep `tools_py/build_dict.py` as the existing
  standalone interface until its behavior and tests are deliberately
  migrated. Call it through its supported CLI rather than importing private
  implementation details or reimplementing its logic. Its current
  implementation writes to a fixed configured file and overwrites it;
  until a safe output/check mode exists, automated generation must run on an
  isolated copy or require explicit approval and capture the diff.
- **Evaluation:** invoke the real C/host implementation and the existing
  test/build systems. Store versioned protocols, normalized token sequences,
  metrics, and raw outputs. The current repository has no Python binding, so
  do not assume one.
- **Device:** treat ESP-IDF, UART console, board reset, flashing, and
  Filebridge as explicit adapters. Discover commands from the running
  firmware's help/capabilities instead of maintaining a stale host-side
  authoritative command catalog. Verify transferred files before use; never
  delete the remote copy implicitly.
- **AI orchestration:** let the AI select among bounded, typed workflows;
  record its plan and actions; validate every candidate with deterministic
  tests and evaluations. Keep shell access, arbitrary network access, and
  direct writes to canonical data outside the default tool contract.

## AI Autonomy and Safety Model

Autonomy should increase only as repeatability and recovery are demonstrated.
Use explicit operation classes:

| Class | Allowed behavior | Default |
|---|---|---|
| Read-only | Inspect source, metadata, tests, and configured local data | Allowed |
| Reproducible local run | Run offline tests/builds and write reports to the external workspace | Allowed |
| Candidate change | Write a scoped patch in an isolated worktree or user-approved branch; never commit or push | Explicit task scope |
| Canonical data change | Modify runtime dictionaries, rules, locale resources, or reference labels | Human approval and review |
| Network/device action | Download external data, query a board, reset, flash, or transfer files | Explicit target and action approval |
| Destructive action | Erase flash, delete source data, change security state, or overwrite unreviewed sources | Prohibited by default; separate explicit authorization and safeguards required |

Every action should have a declared scope, timeout, output limit, and stop
condition. Device automation must identify the intended board and serial
connection, prevent concurrent port ownership, retain raw output, bound
retries, and stop on repeated resets, disconnects, or ambiguous state. A
command appearing in firmware help is not sufficient authorization.

No automatic commit, push, release, remote deletion, flash erase, or
security/efuse operation is part of the target system. A successful AI run
ends with a reviewable diff and evidence, not an irreversible action.

### Bounded AI improvement loop

For each AI-assisted task, make the system follow one explicit experiment
cycle:

1. Record the objective, owned repository area, locale/profile, allowed
   files and operations, and a run budget (time, iterations, or compute).
2. Inspect the relevant implementation and data; identify uncertainty and
   obtain a reproducible baseline before proposing a fix.
3. Form one falsifiable hypothesis and name the exact regression and
   evaluation cases that could disprove it.
4. Make one scoped candidate change in an isolated worktree or other
   reviewable boundary. Do not mutate held-out references to improve a
   score.
5. Run focused tests first, then the agreed evaluation and applicable build.
   Record the exact commands, environment, failures, and output artifacts.
6. Compare against the same baseline. Keep, revise, or reject the candidate
   according to predeclared criteria; do not change metric definitions
   after seeing results.
7. Continue only while the budget remains and the next experiment is
   evidence-based. Stop and report if results are inconclusive, the data or
   license is uncertain, the baseline is unstable, a safety gate is reached,
   or no useful next hypothesis exists.
8. Return the diff, test/evaluation report, unresolved questions, and any
   approval requests. The user decides whether to apply or merge it.

The loop must not optimize only against its training or tuning set, add
exceptions solely to memorize benchmark words, or claim a general
improvement from a narrow sample. It should prefer interpretable,
project-owned changes that survive the regression suite and are consistent
with the documented pronunciation target.

## Existing Material: Retain, Audit, or Defer

Do not migrate the present tree wholesale. Use an inventory table with
owner, purpose, current callers, data inputs, outputs, dependencies, tests,
network/device side effects, provenance, license, and proposed disposition
for every candidate.

- **Root `tools_py/build_dict.py`:** retain as a small standalone tool for
  now. Its options and fixed output behavior are confirmed in source. Add
  safer output/check behavior and deterministic-output tests before
  orchestrating it unattended.
- **Top-level scripts under `ai_developer/`:** review each for active inputs,
  direct source edits, hard-coded paths, actual callers, and repeatable
  tests. Consolidate only scripts that implement the same validated
  workflow; do not rename them into modules solely to fill the target tree.
- **`corpus_miner/`:** treat as an experimental subsystem. Its current
  dependency set includes heavyweight NLP/model and remote-data packages;
  source connectors can download large datasets. Audit each connector's
  actual behavior, source terms, provenance, deduplication, homograph
  evidence, and tests before retaining it. Its aspirational "continuous" or
  "scientifically precise" descriptions are not acceptance criteria.
  Integrate only validated, bounded ingestion and review workflows, with
  each source optional.
- **`data/`, `downloads/`, and corpus outputs:** determine whether each item
  is canonical input, duplicate, cache, generated output, private data, or
  reproducible download. Compare hashes and provenance before moving or
  deleting anything; keep large or restricted datasets out of the Python
  package and default Git history.
- **`host_test/`:** separate maintained C/CMake test source from generated
  build output. Reuse the maintained harness; do not adopt build artifacts
  as application code.
- **`scratch/`, caches, and `corpus_miner/.archive/`:** preserve during
  inventory. Promote only reviewed work with a test and defined ownership.
  Remove generated caches only after their exact paths and reproducibility
  are confirmed.
- **Research and roadmap documents:** use as hypotheses and historical
  design inputs, not current implementation guarantees. Claims such as
  zero-defect behavior, 100% accuracy, a clean licensing outcome, complete
  language support, or a validated unlimited mining workflow require
  independent evidence. Resolve conflicts against current code, tests,
  data provenance, and explicit project decisions.

## Migration Plan and Exit Gates

Work in small, reversible phases. Do not perform a bulk move before the
inventory and baseline exist.

### Phase 0: Protect and inventory

Create a machine-readable inventory of paths, file types, sizes, hashes,
dependencies, imports/callers, tests, data ownership, network effects, and
generated status. Exclude paths explicitly marked not to read or delete.
Record candidate moves and rollback paths. Do not alter source or delete
archives during this phase.

**Exit gate:** each non-generated candidate has an owner and disposition;
large, restricted, duplicated, or generated data is identified; current host
tests and supported firmware examples have a recorded baseline.

### Phase 1: Establish contracts and reproducibility

Define versioned contracts for evaluation inputs/results, data provenance,
run manifests, and error/status reporting. Test existing generators and
host evaluators in temporary copies. Resolve overwrite hazards and stale
tests before automation depends on them. Record which results are
exploratory versus validated.

**Exit gate:** offline tests are repeatable; generated outputs are
deterministic or their variation is documented; failures and missing inputs
are explicit; no test requires an unrequested network or device action.

### Phase 2: Build the smallest usable package

Add the package manifest, thin CLI, minimal core contracts, and one complete
vertical workflow: inspect the project, run the real host tests/evaluation,
and emit a traceable report. Keep dictionary generation behind the existing
CLI until a tested migration proves a direct library interface is better.
Move only code required by that workflow.

**Exit gate:** a clean minimal environment can install and show help, run
offline checks, produce a manifest/report, and run its unit suite without
downloading models or corpora.

### Phase 3: Migrate high-value data workflows

Add reviewed data audits and reproducible dictionary/rule pipelines. Introduce
external corpus connectors one at a time as optional dependencies. Preserve
source identity, license/terms, hashes, transformation lineage, and human
review status. Treat automatic outputs as candidates, not runtime truth.

**Exit gate:** each imported workflow has focused tests, source attribution,
bounded input/output behavior, deterministic or explainable results, and
documented review requirements.

### Phase 4: Add hardware and bounded AI operation

Integrate ESP-IDF, console, UART, and Filebridge through explicit adapters.
Test host fakes first; keep hardware tests opt-in and record the actual
board/build. Add AI planning only after the underlying workflows have stable
contracts and safe failure behavior.

**Exit gate:** operation authorization, timeouts, stop conditions, recovery,
raw logging, and human review have been tested; no destructive operation is
reachable through the default workflow.

### Phase 5: Retire legacy paths deliberately

After migrated workflows have equivalent or better tests and outputs,
deprecate obsolete scripts with a replacement notice. Remove archived or
generated content only after owner review, provenance/data retention checks,
and a verified backup or reproducible source.

**Exit gate:** no active caller depends on a retired path; documentation,
tests, and examples use the supported interface; removal is reviewable and
reversible.

## Definition of Done for the Target System

The AI-developer workspace is ready for routine use only when all of the
following hold:

- One documented, versioned CLI provides stable commands for discovery,
  data audit, evaluation, generation, reporting, and explicitly enabled
  device workflows.
- A minimal install supports help and offline workflows; network, NLP/model,
  and hardware dependencies are optional and isolated.
- The real ESPure C implementation and ESP-IDF/host test harness remain the
  sources used to validate runtime behavior.
- Evaluation protocols identify locale, pronunciation target, corpus,
  reference quality, normalization, metric definition, exclusions, and
  limitations; held-out data is protected from tuning leakage.
- Data and generated artifacts have explicit ownership, provenance, review
  state, and non-destructive handling.
- Every substantive run records its configuration and outcomes in a
  versioned manifest and can report failures without disguising them.
- AI-generated changes are scoped, tested, reviewable, and reversible;
  canonical linguistic data and consequential device operations remain
  gated.
- Documentation states what is implemented, experimental, planned, and
  unsupported without aspirational claims presented as facts.

## Data, Provenance, and Evidence

Treat language data, source code, generated C files, test fixtures, and
evaluation corpora as distinct artifacts with distinct provenance. Where
available, record the source, transformation, version, configuration, and
review status for generated or imported data. Do not invent missing
provenance or treat a generated representation as independent evidence for
the quality of its source.

The German rule source carries an eSpeak GPL-3.0-or-later notice; converting
it to C does not remove that provenance. The active German exception data
also has incomplete per-entry source and review metadata. The runtime
phoneme inventory and IPA map are provisional and have not received an
independent linguistic review. Project-wide licensing and compatibility
obligations remain open; clean-room implementation intent alone does not
settle data rights.

The `en-GB` implementation is a small architecture pilot, not full English
support. Any test or report must state the locale and build configuration it
covers. Validate linguistic correctness separately from structural
consistency: confirming that tokens are declared and renderable does not
prove that their pronunciation is correct.

## Decisions Still Requiring Evidence

The high-level structure and goals above are the recommended target. These
implementation decisions remain open until Phase 0 and Phase 1 provide
evidence:

- Which existing scripts and corpus-miner stages are worth migrating, and
  which are safer to retire.
- Which corpus/reference sources have compatible provenance, rights, and
  sufficiently reliable labels for each evaluation purpose.
- The final evaluation split, normalization, metric implementation, and
  supported locale matrix.
- Whether the package uses only the standard library for its base install
  and which optional dependency groups are justified.
- The Python and ESP-IDF support matrix, determined by actual code and test
  coverage.
- The exact host-side route for invoking the C runtime without introducing
  an unsupported Python binding.
- Which device operations can be made observable and safe enough to
  automate.
- Whether any source-of-truth data should move; no move is assumed by this
  architecture.

Answer these by inspecting and testing the repository, not by treating the
illustrative target tree as a migration order or another project's
architecture as a requirement.

## Related Documentation

- [Project overview](../../README.md)
- [AI-developer architecture review and language priorities](../../tools_py/ai_developer/docs/AI_DEVELOPER_RESTRUCTURING_REVIEW.md)
- [Integration guide](../INTEGRATION.md)
- [User console example](../../examples/console_espure/README.md)
- [Developer console example](../../examples/console_espure_dev/README.md)