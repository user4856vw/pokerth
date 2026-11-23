# Kibitzer Stage B — quantitative bet/raise *what-if* model

Stage A identifies *candidate* premium/value/draw hands without betting EV.
Stage B adds two **explicitly conditional chip-EV calculations**, exclusively
for a **local, heads-up, non-all-in** PokerTH decision.

## Quick start

1. Start a local tournament, reach a decision with exactly one active opponent,
   open **K → RANGES** and optionally assign that opponent's *initial/pre-bet*
   hole-card range. Without a selection, the initial range is **ANY**.
2. Open **K → BET EV**, then enter the opponent's assumed *calling range*
   against a half-pot raise and/or pot-sized raise. Tap **Save**.
3. Inspect each size's actual new chips to spend, derived opponent fold fraction,
   caller equity, calculated chip EV, delta vs check/call/fold, sample count,
   and Monte Carlo sampling margin.
4. **ANY** means the opponent continues with all hands in the initial range
   (zero fold equity). Blank means that candidate is **not calculated**.
5. Continuing ranges must be subsets of the initial range. Hypotheses are per-seat
   and last for the current local game; they do not expose hidden bot cards.

### The model

Let:

- `P` = **current pot before hero's action**, including committed bets;
- `C` = new chips needed to **call** right now (0 for a free check);
- `X` = **additional new chips hero spends** when placing a bet/raise,
  the same amount accepted by PokerTH's Raise control;
- `f` = probability the remaining opponent folds under the supplied
  continuing-range hypothesis;
- `e_call` = hero's showdown pot share *conditional on a call*;
- `e_prior` = hero's showdown pot share against the pre-bet initial range.

If the opponent folds, hero earns the original pot `P` relative to the
decision point (their own fresh raise chips are returned).

If the opponent calls, they put in an additional `X-C`, and the final pot
becomes `P + X + (X-C)`, so

```text
EV_raise(X) = f*P + (1-f)*[e_call*(P+2X-C) - X]

EV_check = e_prior * P                       (if C == 0)
EV_call  = e_prior * (P+C) - C               (if C > 0)
EV_fold  = 0
Reference EV = EV_check if C == 0, else max(EV_fold, EV_call)
Advantage(X) = EV_raise(X) - Reference EV
```

**The modeled sizes are not interpolated or optimized**: if legal, the
calculator considers exactly `X = C + round(r*(P+C))` for `r=1/2` and `r=1`.
The pot used for sizing is the **pot after hero first matches the call**. It
rejects sizes below the legal minimum, at/above hero's remaining stack or which
would force the opponent all-in; it never silently clamps to a different size.

### Where does f come from?

It is *not* a statistic inferred about the actual PokerTH bot.

For every combination of unseen hole cards, remove combos that conflict with
hero's cards or the visible board. Count the combinations compatible with the
user-selected **initial** range and the **continuing** range. Under equal
weights for each remaining combination and a deterministic response rule:

```text
f = 1 - (# compatible continuing combos / # compatible initial combos)
```

A stronger opponent-response model would need evidence of actual calling,
folding and raising behavior conditional on board, stack depth, action history,
position and bet size. Our model currently has none of that.

The separate `e_call` equity is estimated against the continuing range by
the standard PokerTH Monte Carlo evaluator. Since the model selects the
best of two sizes, the displayed Monte Carlo sampling guard uses an approximate
two-comparison familywise normal multiplier (2.25) on the conservative **sum**
of the baseline and candidate EV standard errors. This does not imply that
the strategy is 95% likely to be correct.
**It does not measure the potentially much larger error in the selected ranges.**

### Numerical example (not a prediction)

Suppose `P=100, C=0, X=50, e_prior=0.60, f=0.30, e_call=0.70`.

```text
EV_check  = 0.60*100 = 60
EV_raise  = 0.30*100 + 0.70*[0.70*(100+100)-50]
          = 30 + 0.70*(140-50) = 93
Advantage = +33 chips relative to checking
```

This +33 result has meaning **only if** the continuing range and its
implied 30% fold rate are credible and the terminal-showdown simplification
is accepted.

### Limitations — deliberate

- One remaining opponent only: no multiway or side-pot EV.
- Neither player can be forced all-in in a candidate raise.
- Opponent can **call or fold only**; no re-raise or bet on subsequent streets.
- No rake, implied odds, position advantage, GTO solution, tournament
  Independent Chip Model (ICM) or prize-money optimization.
- Calling ranges are **preflop hole-card types**, not board-dependent policies.
- This is incremental **chip** EV, not a measure of poker skill or true tournament EV.
- The feature **never sends poker actions automatically**.
- The live Phase-B numbers are **not stored for seeded historical replay**; existing
  decision snapshots retain their original equity replay behavior.

### Manual QA after Android build

1. Against one active opponent, set initial range to ANY, half-pot calling
   range to `ANY`. Verify 0% derived folds.
2. Set a nontrivial half-pot continuing range (e.g. `99+,AJs+,AQo+,KQs`);
   verify the displayed count and fold rate update.
3. Narrow the initial range to `AA`, then try a calling range `KK`;
   verify that validation rejects a non-subset.
4. Confirm that preflop/rivers with no legal non-all-in candidate show
   an unavailable reason rather than a made-up size.
5. Check that multiway and all-in situations display no numerical bet advice.
6. When facing a bet, verify `opponent additional call = X-C` and the
   pot-after-call sizing convention from the detail panel.
7. Verify that Stage A recommendations, tournament statistics, archive
   backup and seeded equity replays continue to work.


---

## Strategy council (modeled expert-style cross-check, v1)

The live Kibitzer adds a tappable **MODEL COUNCIL** row below BET EV.
It reports one of:

- **NO COMPLETE ACTION COMPARISON** — only the simple equity and
  pedagogical heuristic checks exist; there is no full action ranking.
- **CONDITIONAL RAISE EDGE** — Stage B found one of its two modeled
  raise sizes better than the passive reference beyond sampling error,
  conditional on the user's continuing-range assumptions.
- **TESTED PASSIVE: CALL/FOLD/CHECK** — Stage B found both tested
  raises inferior to its passive reference, and the separate
  pot-odds check resolved which passive alternative is supported.
- **BET EV: NO DECISIVE EDGE** — the model cannot distinguish the
  tested raise from the passive action beyond its sampling uncertainty.

Tap the row to inspect four named educational lenses:

1. **Pot-odds discipline:** only compares *CALL versus FOLD*
   (or notes CHECK is free) using `call / (pot + call)` and the
   displayed margin. It never asserts that calling beats raising.
2. **Assumption stress test:** compares this narrow CALL/FOLD judgment
   against uniform-random cards and the user's selected opponent
   range. It identifies range-sensitive findings; **it is not an
   additional independent vote**, because it reuses pot-odds equity.
3. **Value and pressure scout:** reports Stage-A premium/value/draw
   situations worth studying as a possible raise. A candidate is
   **not** numerical positive EV and never counts as a decision vote.
4. **Response-aware chip EV:** only when the user has supplied
   appropriate heads-up continuing ranges. It tests two concrete
   sizes against the passive reference, reporting an edge only
   beyond the sampled margin. A "raise better than call" conclusion
   is perfectly compatible with "call better than fold."

These are complementary comparisons, not four independent competing
action votes. The UI deliberately does not manufacture a percentage
of consensus or a false disagreement between non-exclusive results.

### Critical provenance limitation

**No real world-class professional players have been consulted.**
There are no named expert votes, professional endorsements, live
opinions, verified player-style models or GTO solver outputs. The
lenses are transparent deterministic checks; the same sampled
equity supports more than one lens. Actual pro consensus would
require verifiable expert-by-hand annotations or validated solver
data, including provenance, positions, stacks, betting sequences,
board textures and opponent ranges.

The full council output and recorded inputs are stored in each new
hand-decision snapshot. They can be inspected after later range edits.
Existing **REPLAY CALCULATION** verifies seeded equity samples; it
does not independently reproduce Stage-B EV or the council's
missing assumptions. No opponent hidden cards are read; the
feature remains local-only and never triggers a poker action.

### Acceptance checks

1. With no continuing-range response model, display **NO COMPLETE
   ACTION COMPARISON** even if pot odds strongly favor calling.
2. Assign a tight opponent range that reverses the priced
   CALL/FOLD result from the random baseline; display a range
   sensitivity warning in the council details.
3. For a heads-up response model supporting a significant raise
   advantage, display **CONDITIONAL RAISE EDGE**, regardless of
   whether the separate pot-odds check favors calling over folding.
   Never call these findings contradictory.
4. If raises are inferior beyond the sampling margin, display
   **TESTED PASSIVE** and its supported CHECK/CALL/FOLD action.
5. If Stage B is within sampling error, report **NO DECISIVE EDGE**.
6. A Stage-A aggression candidate may say **STUDY A RAISE** but
   never casts a fictitious full-action vote.
7. Tapping the row exposes assumptions, baseline and conditioned
   equities, call threshold, margin and per-lens explanations.
8. New saved decisions retain their original council evidence.
   Pre-feature saved histories load and replay without it.
