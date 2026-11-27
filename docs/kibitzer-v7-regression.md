# V7 local autoplay regression candidate

Base: c6bb7119e5518e151904170683dc3c1dfdadec42 (P0 v6).
Source incidents: November 26–27, 2026 overnight JSON; the historical
outcomes are not used to select the action. The original legacy policy is
retained as an auditable counterfactual.

## Changes and justification

* Call/fold after aggression uses the existing three conditioned equity
  scenarios and their sampling envelope when a valid eligible pot is
  known. Positive showdown chip EV means e*(P+C)-C > 0. No new
  fraction-of-stack veto. The existing 1.96 multiplier approximates
  a two-sided 95% normal sampling interval per scenario, not simultaneous
  coverage or confidence about the true opponent range.
* Count actual price increases from the public journal, including hero.
  Two prior increases mean a subsequent raise is at least a fourth bet
  (the blind is the first bet); short all-ins may not reopen action.
  Do not initiate such a raise with only the old three-bet chart.
  Preserve the inherited AA exception.
* In preflop multiway all-in states, reconcile all current street
  contributions to the pot; cap each opponent's contribution at hero's
  final contribution. Global showdown share versus all active opponents
  lower-bounds the share in each eligible tier, assuming no further betting.
  Never use this lower bound to assert negative EV for a side-pot call.
  Missing or inconsistent ledger => no promotion.
* Suppress unmodeled half-pot probes on the river or paired/three-suited
  boards. These are structural poker conditions, not fitted cutoffs.
  Preserve explicitly modeled raises. Other legacy dry flop/turn probes
  remain provisional.
* Shove break-even fold probability is exact under a supplied single-caller
  hypothesis: called EV = e*(P+S+R)-S; f = max(0,-calledEV/(P-calledEV)).
  The audit reports the zero-equity diagnostic using an upper bound on
  matched risk. It is not a forecast of the actual fold frequency, an EV
  relative to checking, or a new decision threshold.

## Remaining work / release scope

The three opponent likelihood priors, legacy short-stack ranking/opening
charts and remaining stack-fraction fallbacks are not calibrated.
The new call is a provisional showdown comparison, not a solution to
future betting, position, equity realization, or ICM. No empirical
calling probabilities are available; automatic opening shoves still
use the legacy chart. A full tier-specific postflop EV model and
response-aware bet sizing are outstanding. Manual ranges still use
the inherited preflop policy except for the later-raise guard.
The QQ follow-up is a counterfactual test once the earlier action changes;
a conditional snapshot replay cannot establish counterfactual tournament
finishes. Existing P0 regressions remain intact. CI and the APK build
both run the incident regressions before producing a candidate.
