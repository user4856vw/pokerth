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

The live Kibitzer adds a tappable **STRATEGY COUNCIL** result under
BET EV. It reports either `MODELS ALIGN: <action>`,
`MODEL DISAGREEMENT`, or `INSUFFICIENT COMPARABLE MODELS`.
Tap it for four lenses and their detailed assumptions:

1. **Pot-odds discipline:** compares conditional showdown equity with
   `call / (pot + call)`, with the displayed caution margin. With a free
   check, `CHECK` is merely a cost-free fallback, *not* proven optimal.
2. **Assumption stress test:** with manually assigned ranges on a priced
   decision, compares the random-opponent baseline against conditioned
   equity. This check counts only if both clear their margins on the
   same action. If they disagree, it explicitly reports sensitivity.
3. **Value and pressure scout:** surfaces the existing Stage-A premium,
   value or semibluff opportunity to *study* a raise. This is never
   counted as an actionable `RAISE` vote: no opponent fold/call EV has
   been computed.
4. **Response-aware chip EV:** only when the user configures opponent
   continuing ranges and Stage B produces a supported result. This
   lens can prefer a modeled bet/raise, favor the passive reference, or
   abstain when results remain within simulation uncertainty.

The council calls a result **aligned** only when two or more *comparable*
checks select exactly the same action **and no comparable check
contradicts it**. It never treats a majority as unanimity. The response
model is heads-up only, and its two preset sizes are not a full
optimization.

### Critical provenance limitation

This is **not a survey or consensus of named world-class professional
players**. The four lenses are transparent deterministic heuristics;
some reuse the same sampled equity and so are **not independent expert
opinions**. No actual poker pro has been consulted, no player-specific
strategy has been reproduced, and no game-theoretic-optimal (GTO)
solver is involved. Actual expert consensus would require verifiable
hand-by-hand expert annotations or a calibrated solver dataset, with
provenance and coverage of different positions, stacks, action
sequences, and board textures. The UI makes this distinction explicit.

The full council output and inputs are saved with each new decision
snapshot, so they remain inspectable even when current ranges change.
Existing `REPLAY CALCULATION` checks seeded equities, **not** complete
Stage-B EV or the council's underlying hypotheses.

### Android acceptance checks

- With no manual ranges and no Stage-B response model, the council
  must display **INSUFFICIENT COMPARABLE MODELS**, not fabricated agreement.
- With both equity hypotheses safely favoring a priced call and the
  response model unavailable, the council must show **MODELS ALIGN: CALL**;
  the detail must explain that the stress check reuses inputs.
- When assigned-range equity favors a call but the random baseline
  clearly favors a fold, display the sensitivity warning and no
  model-wide agreement without further evidence.
- When Stage B favors raising while the pot-odds check favors calling,
  display **MODEL DISAGREEMENT**, even if two related passive checks agree.
- A Stage-A draw candidate must appear as **STUDY A RAISE**, but it
  cannot itself create a raise consensus.
- Tapping the row must display the equity values, threshold, margin,
  input assumption statuses, per-lens reasoning and source limitation.
- New saved decisions expose their original council calculations under
  the decision-detail view; old history lacking council data remains valid.
