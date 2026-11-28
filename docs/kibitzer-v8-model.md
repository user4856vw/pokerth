# V8: public-action frequency constrained stress test

Base: V7 47469e04d768b95dc82378aa07cdc23feed64446. This is a local
experimental policy, not a solver, a professionally validated opponent
model, or a model of real-money returns.

## Data and identification

Training: sessions 17 and 18 of the November 27/28, 2026 history export
(1964 hands); validation: sessions 19/20 (1958 hands). Use the longest
captured public opponent-action prefix in each hand. Do not count
the same action at each hero decision. Count raise opportunities only
when stack > call price. Exact counts and reproduction script:
scripts/kibitzer_v8_action_counts.py. The frozen table is from V6 only.

| Context | V6 raises/opportunities | V7 raises/opportunities |
|---|---:|---:|
| Unraised preflop | 703/4976 | 730/5173 |
| Facing raise, no earlier own aggression | 99/1889 | 102/1924 |
| Facing raise, previously aggressive | 7/27 | 9/30 |
| Flop free action | 19/85 | 27/151 |
| Turn free action | 17/72 | 21/130 |
| River free action | 17/51 | 24/78 |

These are captured action frequencies, not complete per-player action
rates or true ranges. The journal ends at the last hero decision.
Responses that end the hand may be absent, and observations are correlated.
There are no opponent hole-card labels in these logs. A Wilson endpoint
is a sensitivity device for sparse counts, not a valid population
confidence bound under censoring. No future cards or results are fed
into runtime inference. The current visible board and hero blockers are
used to exclude impossible combos even when evaluating earlier actions;
this is an approximation to a full sequential card-removal likelihood.

## Model

Order preflop classes by an independently generated HU uniform-opponent
reference equity (100,000 deals per 169 classes, seed 20261128 + class
index; worst-case per-class MC standard error 0.001581, not a new
strategy threshold). Rank postflop by current made-hand evaluator.
Rank is a value proxy, not a draw-aware or position-specific solution.
Use the CDF of the weighted prior for each observation. With randomized
rank u uniform on [0,1] and marginal action frequency q, the two
selection hypotheses are L(u)=u^(1/q-1) and L(u)=1[u>1-q].
Their integrals equal q. Tied ranks are integrated over their entire
probability-mass interval. Use the broader Wilson endpoint q to avoid
point-estimate tightening with sparse data. Call = observed continuation
(coarsened); checks leave the range unchanged. No arbitrary weak-hand
aggression floor. Both hypotheses emphasize value and may underestimate
semi-bluffs, polarized raises, or future equity. The original broad
V7 estimates remain in the audit, not asserted to be calibrated.

## Call/fold

For each compatible joint deal, calculate all hero-eligible pot tiers
separately and subtract the incremental call price. Include folded
contributions in tier amount, not eligibility. Reconstruct prior street
commitments using public action journal maxima and current street sets.
If total does not equal engine pot or no active villain matches final
hero stake, abstain. A single-pot heads-up turn/river is exactly
enumerated. Multiway deals use the inherited rejection budget and
sample minimum. These are showdown projections with no further betting,
NOT continuation EV or ICM. Do not use global multiway equity as a
negative-EV proof about a smaller side pot.

For V7 calls only, compare the maximum regret of calling
(max(0,-minimum scenario EV)) with folding (max(0,maximum scenario EV)),
including the existing 1.96-SE sampling envelope. Fold if regretCall >
regretFold; ties retain V7. This is a declared experimental risk
criterion, not an empirically identified preference. No new call
promotions, preserve preflop AA, manual ranges, free checks and all
modeled raises. Invalid/unsupported inference retains V7 and is logged.
Opening shoves still use existing V7 charts. No new stack-fraction veto.

## Bet EV diagnostic

For a single proposed HU probe, compare check-down share e0*P with
(1-q)*P + q*(ec*(P+2B)-B), where ec is equity versus the selected
continuation range. Test the captured response rate and Wilson endpoints;
the lower scenario additionally subtracts an upper-rate sensitivity for
reraises (at most P+2B loss vs the called branch). No automatic value-bet
change is made because observed responses are very sparse/censored and
future betting is unmodeled; zero river responses means unavailable,
not zero fold or raise probability. Candidate is logged even when V7
suppresses it. A full multiway response model, calibrated bluff/continuation
likelihoods, equity realization, shove EV and ICM remain outstanding.

## Tests

CI and APK gates run old P0/V7 tests plus V8 tests. V8 covers probability
mass/tied ranks, independent reference ranking check, side-pot ledger and
payoffs, JJ/KJs/KJo new incidents and old KK/QQ. Missing ledger/range
does not fabricate an action. This is not a counterfactual tournament
simulation or statistically established performance improvement.

## Improved evidence collection

V8 also stores the public action journal at settlement, including hero and
opponent actions after the last hero decision (even if hero folded). This
is offline evidence only. Future calibration must distinguish these
complete journals from earlier censored prefixes. The response diagnostic
is not evaluated when the single villain cannot cover the proposed bet.
