# Kibitzer Monte Carlo sample-count preference

## What changed

The Kibitzer now exposes **MONTE CARLO** immediately beneath its top toolbar.
Tap it to open the sample-count editor; choose **1,000 through 10,000
samples per equity scenario** in 500-run slider steps, or use the 1,000,
2,500, 5,000 and 10,000 presets. Press **Apply** to save.

- Default remains **2,500**, matching earlier trainer builds.
- The setting is persisted with Qt `QSettings("PokerTH","KibitzerTrainer")`
  under `monteCarloRuns`, **separately** from the training-hand JSON archive.
  Backing up/restoring histories does not modify the MC preference.
- All *Kibitzer* live Monte Carlo calls use the same selected number:
  1. Unrestricted/random-opponent baseline equity.
  2. User-assigned-opponent-range equity.
  3. Stage B: equity conditional on calling the half-pot raise.
  4. Stage B: equity conditional on calling the pot-sized raise.
- When a range is impossible or conflicting, fewer than the requested number
  of valid samples may be generated. The user interface shows both counts.
- No change to the opponent AI's own calculations or game decisions.
- Deterministic replay uses the **original `samplesRequested` and seed
  from each hand's saved snapshot**; changing the current MC preference
  does not mutate prior hand history or its replay outcome.
- As before, Stage B scenario output is not included in replay packets.

## Recompute and performance

After saving a setting during your own turn, the Kibitzer recomputes
live advice with the new sample count. If only one equity hypothesis is
being evaluated, the total amount of simulation is close to that number.
With a restricted opponent range and both Phase-B sizes enabled, up to
**four separate Monte Carlo calls** may be performed for the same decision.
Thus **10,000 per scenario may mean approximately 40,000 simulated deals**
and cause noticeable UI latency on some Android devices.

The sampling margin decreases roughly as `1/sqrt(N)`, holding other
factors constant; 10,000 instead of 2,500 roughly halves the sampling
standard error. Neither run count establishes whether user-supplied
opponent ranges, fold responses or ICM assumptions are correct.

## Acceptance checks

1. On a fresh install, label displays **2,500**.
2. Select **1,000**, Apply, and inspect equity details during a local
   turn: requested sample count is **1,000**.
3. Select **10,000**, Apply, and inspect both random equity and
   manually assigned range equity; both request **10,000**.
4. With heads-up BET EV enabled, each evaluated size reports
   `samplesRequested = 10,000`; low compatible count still shows
   an insufficiency warning rather than silently replacing the range.
5. Exit and restart the app: the last selected count is retained.
6. Replay a hand recorded with 1,000 samples *after* changing the live
   preference to 10,000: replay must still use 1,000 and report MATCH.
7. Confirm no MC setting controls are exposed on external online
   games, where the local Kibitzer itself is unavailable.
