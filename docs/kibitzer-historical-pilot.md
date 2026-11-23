# Historical professional-hand pilot — 30 days

## Purpose and provenance

This is a **source-linked retrieval feature**, NOT a professional opinion poll,
a GTO solver, an equity engine or an optimal-action grader.

The initial immutable pilot corpus is:
- **11 no-limit Texas hold'em hands**, **88 individual recorded decisions**,
  selected by the *variant = 'NT'* filter from the 2023 World Series of Poker
  Event #43 ($50,000 Poker Players Championship, Day 5), a mixed-game final
  table. The remaining hands of that television recording are **not NLHE** and
  are excluded entirely.
- Curated from the University of Toronto / Universal, Open, Free, and
  Transparent Computer Poker Research Group **Poker Hand History (PHH)**
  dataset at exact upstream revision
  `e47fbd5816372360bade4de5d712346fe1bb70f6`.
- Original GitHub dataset: https://github.com/uoftcprg/phh-dataset
- PHH notation specification:
  https://phh.readthedocs.io/en/stable/required.html
- PHH dataset verification notes:
  https://phh.readthedocs.io/en/stable/validation.html
- Original recordings / broadcaster copyright is **not** conveyed by
  the repository's MIT license. This catalog contains succinct factual
  observations with individually linked PHH sources, not broadcast media.

Every matched example has a **specific original source URL** (pinned to a
commit), player, hole cards, recorded action, street, position, estimated
remaining stack, pot and call amount in big blinds. The compiler bundles
the catalog as a Qt resource: it is available offline and never depends on
a flaky live website or scraping.

The game does NOT inspect unrevealed cards of its current opponents and
does NOT access external real-money poker clients.

## Using the feature

1. Start a local tournament and reach a decision.
2. Open K -> **HISTORICAL PRO HANDS**. If a structural analogue exists,
   inspect the *observed* historical action, context and original source.
3. If no close analogue exists, the app will say so. It may list a few
   weaker examples as **CONTEXT ONLY**, never as a recommendation.
4. Open K -> **MODEL CROSS-CHECK** to see the separate mathematical
   heuristic calculations. Do NOT count this as another player vote.
5. Play normally. Before any resulting board or outcome is known,
   the app records a **compact audit** of whether a historical structural
   match was present when you actually made your decision. It does not
   record or upload the source catalog repeatedly.
6. Open K -> **PROGRESS -> 30-DAY HISTORICAL HAND PILOT** to review the
   rolling 30-day counts.

## What a similarity score actually means

Hard gates require the same street, the same "facing a call" versus
"free check" state, the same heads-up/multiway category, the same
pocket-pair and suitedness class, a reasonably comparable rank class,
and stack depth within about 2.5x. Also, postflop hands must match
whether the board is paired, its suit texture, the hero's board-rank
hits and broad draw/made-flush states.

Further score deductions cover position, exact ranks, pot size,
remaining stack and bet-to-pot pressure. A score of **82+** is labeled
a structural analogue; anything lower is explicitly contextual.

**This 0–100 number is not a statistical confidence, probability
of a correct play, success metric, or expert endorsement.** The
classifier does NOT reconstruct a full betting action sequence,
position-dependent opponent ranges, payout/ICM, reads or future
betting. Similar-looking card patterns can have very different EV.

Only source actions are shown — not aggregate percentages of how
"pros would play" a situation, because there is no representative
denominator and 11 final-table hands are severely selection-biased.
Two historical actions must not be mistaken for independent "votes".

## Month-long assessment

Record at least **30 days of ordinary local play** before assessing
the feature. The counters are computed locally from decisions in the
rolling last 30 days; the existing history cap applies (2,500 hands).
No scheduler, cloud sync, uploads, or automatic sharing is involved.

The dashboard shows:
- `decisionsChecked` — decisions for which a historical search ran,
- `structuralMatches` / `noCloseMatch` — coverage versus abstention,
- `distinctHistoricalExamples` — whether retrieval repeatedly
  chooses the same WSOP hand,
- `sameObservedAction / comparableActionPairs` — purely descriptive
  action overlap. **Not an accuracy or skill score.**

Suggested acceptance questions after one month:
1. Is the feature available offline and does every example link to an
   original PHH file?
2. Are the displayed pot and stack figures sensible and traceable?
3. Does it abstain in obviously different situations?
4. How often is a genuinely comparable example found?
5. Does opening a source lead to a useful learning question, or merely
   provide amusing anecdotes?
6. Do you repeatedly see the same 1–2 hands, indicating an urgent
   need for a larger and more diverse legally usable corpus?

The expected failure mode is **too few matches**, not excessive claims
of professional agreement. This is deliberate. If coverage is poor,
expand the corpus with sources that include all actions and sufficient
context, and validate source rights before claiming new evidence.

## Regeneration and test coverage

To reproduce the source-derived facts, use Python 3.11+ and
`scripts/build_kibitzer_pro_corpus.py` on the checked-out upstream
PHH dataset at the **pinned revision**. The converter has an optional
`--check` mode to compare source-derived facts to the bundled JSON.

The GitHub APK CI executes:
- `scripts/validate_kibitzer_pro_corpus.py` (provenance, exact records,
  card syntax, source URLs, unique IDs and game type),
- `tests/kibitzer/test_pro_matcher.cpp` (same-street and stack/range gates,
  action price, position, suits, hole ranks, and postflop board textures),
- the existing action, equity-range and Monte Carlo tests.

The feature fails closed if the bundled catalog does not validate.
