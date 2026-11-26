# P0 strategy correction (v6)

Separate branch, based on v5 evidence infrastructure. v4 remains frozen as the
overnight baseline. No hidden cards, opponent-private bot state or future board
cards enter the local autoplay decision.

## What is implemented

* `kibitzer_board_analysis.h`: reuse engine HandEvaluator for 5/6/7 cards;
  separate private made category from board-supplied rank; count rank/suit
  texture and one-card straight/flush improvements. For every such out,
  enumerate compatible opponents to calculate the next-street showdown share
  and whether any opponent can beat the improved hand. On the flop this is
  only a next-street diagnostic, not final river winning probability.
* `HandInterface::publicBetActions()`: per-hand action records appended at
  the settled LocalPlayer::action boundary, with seat, street, action, amount
  paid, pot before, highest set before, player commitment, stack before and
  players still in hand. No opponent hole cards recorded. Hand objects are
  freshly allocated per hand. Only local autoplay consumes these records.
* `kibitzer_opponent_model.h`: normalize posterior combo weights by
  multiplying action likelihoods (based on rank percentile, drawing
  potential, bet size and aggression). Three explicitly **uncalibrated**
  profiles: balanced, value-heavy, bluff-heavy. Known cards removed from
  every seat's candidates. Multiway simulations use whole-deal rejection
  to avoid colliding cards and seating-order bias. Heads-up on turn and
  river, enumerate all compatible opponent combos and possible runouts
  exactly, including ties. Exact numerical results are conditional on the
  chosen prior/likelihoods, not a claim about real opponents.
* `kibitzer_strategy_p0.h`: preserve AA and sub-2BB survival, reject
  speculative expensive preflop calls to exceptional raises and multiway
  price deficits; permit robust cheap heads-up all-in calls. Postflop,
  compare conditioned equity with eligible pot odds and consider board
  texture, private made hand and stack exposure. In high-exposure marginal
  situations require robust support across profiles; do not blindly trust
  unconditioned equity. No automatic large aggressive probe with a
  board-only hand on a threatening board.
* Heads-up position: engine's LocalHand::assignButtons overwrites dealer
  with SB in heads-up play. Recover dealer via unique SB and BB markers.
* Journal: `autoplay.strategyP0` has old decision **on corrected state**,
  new decision/reason, exact/model estimates, RNG seed, board assessment
  and public action records. Prior v5 external opening-chart comparisons
  still logged; no charts automatically control this policy.

## Limits and verification

* Local AUTO TEST only; normal human-facing adviser still has original
  manual/unknown-range display. Manually entered range hypotheses are not
  overwritten.
* The posterior is a provisional engineering model. Position-specific
  source priors, likelihood calibration from external data and actual
  opponents, full ICM, future betting/realization and multiway side-pot
  EV remain unfinished. Do not interpret profiles as calibrated
  confidence intervals. Board diagnostics do not count every possible
  runner-runner or pair/full-house improvement as a straight/flush out.
* For heads-up pot odds subtract the uncallable excess bet. For uncertain
  multiway all-ins do not use one pot-share EV to promote a fold.
* Historical QJ/A9 cases replay exact public cards, prices and stack sizes
  with an explicitly reconstructed illustrative same-aggressor sequence;
  previous exports lack opponent-specific action history, not actual
  revealed cards. The new recorder is intended to remove that ambiguity.
* The old decision logged is not an exact counterfactual replay of v4:
  position is corrected, and RNG/game trajectories differ.
* Unit CI links the same engine evaluator as the APK; complete Android
  compilation and on-device verification are separate gates. Preserve
  frozen overnight versions and export JSON before updating.

Run the engine-linked isolated tests:
```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
  -I src/gui/qt6-qml/cpp -I src/engine/local_engine \
  tests/kibitzer/test_strategy_p0.cpp src/engine/local_engine/handevaluator.cpp \
  -o /tmp/kibitzer-p0
/tmp/kibitzer-p0
```
