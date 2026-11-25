# Kibitzer evidence corpus — source-backed, not source-controlled poker

This work builds a **reproducible, strictly read-only reference stream** before
any external information is allowed to change automatic decisions. Branch
`experiment/kibitzer-evidence-corpus-v5` extends v4 without modifying the
v4 APK or its trial. It logs under `kibitzer-autoplay-v5-shadow`.

## Evidence is not fungible

| Tier | Source | What it can support | What it cannot |
| --- | --- | --- | --- |
| A: mathematics | Exact pot odds, deck enumeration, equity given explicit ranges | Consistency checks, bounded chip-EV calls | Future betting, implied odds, ICM, unknown opponent ranges |
| B: reference strategy | `jensbaagaard/poker-practice`, MTT_GTO open charts, depths 5/10/20/40/100 | External frequencies for specified RFI spots | *Our* optimal action in other table sizes or with other antes/opponents/payouts |
| B: HU solver, **candidate only** | George Li, CC BY 4.0, `push-fold-heads-up` | Equal-stack heads-up, SB push vs BB call, no ante, chip EV | 3–10-player decisions, unequal stacks, ICM or nuanced raises |
| C: real action samples, **candidate only** | UofT/UOFTCPRG PHH (MIT), including Pluribus/WSOP and older cash-game hands | Actual historical betting actions/lines and behavior patterns | Correctness or counterfactual EV of unchosen actions |
| D: educational questions, **candidate only** | tyloo/poker-quiz, MIT; Harrington workbook if user supplies problem text | Human-readable problem scenarios/explanations | Solver proof; copying copyrighted workbook exercises wholesale |

A published chart's "GTO" label is a source label, **not independently
verified equilibrium quality**. No amount of observational hand history may
convert actions into independent expert votes. Never weight all these
datasets as if they were i.i.d. measurements of a single latent "correct move."

## First pinned import: MTT *opening* references

Source: https://github.com/jensbaagaard/poker-practice,
`data/openSourcePokerData/MTT_{5,10,20,40,100}_GTO.json`.
The upstream README states MIT and expressly permits reuse of range data;
its documentation describes 6, 8 and 9 player tables. We preserve
attribution and each original Git blob hash in the generated header
`src/gui/qt6-qml/cpp/kibitzer_evidence_open_ranges.h`.

The importer decodes 169 canonical starting-hand classes, weights pairs=6,
suited=4, offsuit=12 (1326 total combinations), validates each frequency
is in [0,1], and stores thousandths as `uint16_t`. Validations reject
unexpected hand codes and missing opening positions. Only **Open<position>**
frequencies are included: do not interpret any `3Bet`, `Call`, `4Bet`,
`5Bet` entry without an opener identity and the requisite conditional history.

The reference is *applicable* only for:

1. 6-, 8- or 9-handed positions **derived from the living dealer-relative
   seat ring**; hero BB cannot open first-in.
2. Nobody previously raised, limped, or went all-in.
3. Actual stack depth is within max(0.25BB, 5%) of one of the reference
   depths **5, 10, 20, 40 or 100BB**. Otherwise it remains a labeled
   source-depth mismatch. Do not silently interpolate or extrapolate.
4. Cards are two distinct valid PokerTH card codes.

Each preflop journal event now records in `autoplay.preflopReference.externalOpeningReference`:
source identifier, applicability status, reference depth and position,
opening frequency and (where near-pure) whether the existing policy
agrees or diverges. Mixed frequencies are reported as mixed; they are
**not** graded as right/wrong and no observed discrepancy automatically
changes the autoplay action.

The raw source data's provenance is preserved. Converting a 2026 external
range snapshot into a claim of statistically proven performance would be
incorrect. Any future policy activation requires matching the tournament
structure (especially antes and ICM), checking the hand class and prior
betting sequence, tracking out-of-sample errors, and running A/B trials
across common random seeds rather than reacting to a single result.

## Planned ingestion that is deliberately not yet active

1. George Li's independently documented heads-up push/fold set:
   https://georgelipoker.com/data/push-fold-heads-up.json
   CC BY 4.0, George Li, georgelipoker.com. The published solver applies
   to **two equal stacks**, no ante and chip EV; independently validate
   file schema and fixed numerical fixtures before bundling.
2. Imported PHH action histories, with strict separation of observed
   actions, ground truth and known public information. Use realistic
   action-conditioned opponent ranges, *not* professional hand history as
   a normative label. Also watch out for cash vs tournament mix.
3. Original Harrington-style questions scored by reproducible reference
   calculations and explicit assumptions; avoid copying the workbook.
4. Evidence-based policy changes must specify measurable failure
   hypotheses (e.g. the KTo fold at 0.87BB) and a test that *would fail*
   if the rule regressed again.

## Reproduction

Unit test:
```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
    -I src/gui/qt6-qml/cpp \
    tests/kibitzer/test_evidence_open_ranges.cpp -o /tmp/evidence_open_ranges
/tmp/evidence_open_ranges
```

The isolated C++ test does not establish that Android packing, live
QML interaction or end-to-end journal persistence works. Keep exporting
JSON after each overnight run, and retain the exact commit SHA and
settings with the outcome. No network request or live remote dependency
is made by the APK.
