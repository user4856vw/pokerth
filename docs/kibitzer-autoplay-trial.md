# Kibitzer AUTO TEST: an auditable local self-play experiment

AUTO TEST is deliberately **opt-in** on the local game table. Start a local
tournament with the desired settings, then tap **AUTO** beside the K button.
Tap **STOP** to return to human play. It cannot act in a network game.

The app plays every hero decision automatically, advances paused hands and
starts a fresh local tournament after an elimination/win. The run stops after
eight hours (wall clock) or 100 completed tournaments, whichever happens first.
It does not restart itself after the app is closed.

Each actual action is recorded in an **append-only, private v2 JSONL journal**.
The old `kibitzer_training_v1.json` is preserved as an independent recovery
copy during the first migration. Each settled hand and tournament result is
written separately; we no longer reserialize the entire history after every
hand or stop persisting when the whole archive reaches 8 MiB.

The GUI retains a rolling window of 2,500 settled hands and 1,000 tournament
finishes for responsiveness; **the journal itself retains all records** until
the user removes application data or the device runs out of storage. On Android,
use **K → PROGRESS → Local history backup → Save history JSON to file…**.
The exporter streams the **complete journal** into the original JSON schema,
not merely the GUI window. Keep independent backups before replacing an APK
with a different signature or uninstalling. A malformed journal fails closed
rather than silently resetting its counters.

Each action's `autoplay` record includes policy version (v3 for the revised\nriver and A-A rules), reason,
`provisional`, and the observed Council stance. Preflop records additionally
include the button index, number of live players, blind, number of limpers
and all-in opponents, and whether an earlier raise was observed.

## Exact decision mapping (autonomous LOCAL play only)

* **Preflop:** do **not** compare equity against all random opponents with the
  immediate call price to decide whether to open. Use a deterministic,
  position-sensitive reference range. Distinguish unopened pots, limpers,
  a prior raise and all-in opponents; choose a legal ~2.5BB open, a cautious
  isolation/3-bet, call, check or fold. Strong but short stacks can shove.
  This is explicitly **provisional**, NOT GTO or an ICM solver; the ranges
  will need calibration against a reliable reference strategy.
* **Postflop, modeled raise:** if the Council supplies a conditional
  bet-EV edge and a valid, legal size based on explicitly configured
  response ranges, use it. No claim is made that those ranges are correct.
* **Postflop, unpriced Stage-A aggression:** a Stage-A signal alone never
  licenses a sequence of minimum raises. A first-in, free-action value
  probe is allowed at approximately half the pot only when sampled equity
  is at least 82%, there are at most two opponents, and the amount is
  legal and below the current stack. Otherwise check when free. These
  probes remain **provisional**, because opponent fold/call behavior is
  still not modeled.
* **River counterraise:** if an opponent raises after the autoplay policy
  has already bet/raised on the river, do not re-raise without a credible
  opponent-response model. A **small provisional bluff-catch call** is
  permitted only when hero's hole cards contribute showdown value AND the
  call is at most 12% of the existing pot AND at most 6% of remaining
  stack. Only heads-up situations qualify, and only one paid call per
  river sequence is allowed under this shortcut. These caps are initial
  risk-control parameters, *not* inferred
  opponent bluff rates or positive-EV guarantees. Expensive counterraises
  are folded; the $1,557 river call from the first overnight trial is
  explicitly covered by regression tests.
* **Pocket aces preflop:** in the ordinary chip-EV trial, A-A has a specific
  value-raise/call rule across open, limped, raised, short-stack and
  already-all-in situations. It is not delegated to the generic
  all-in-defend/fold heuristic. This does *not* model special tournament
  payout/ICM cases, such as satellite bubbles.
* **Other paid decisions:** the Council stance is used where fully
  executable; otherwise the random-range pot-odds call/fold test remains
  a clearly marked provisional fallback. Its limitations still matter.

This is a **testable completion policy**, not a claim that Council is a
fully specified game-theoretic strategy. For the month-end review, report
provisional and directly modeled actions separately; compare results with
a baseline bot at the same blinds/stacks, and distinguish chip outcomes
from decision quality. Opponent cards are never accessed by Kibitzer.

## Running it overnight

AUTO TEST uses the ordinary local game's Qt/QML timers. Android may suspend
a backgrounded app, and a locked screen is not an assurance of uninterrupted
execution. Leave the device charging with the app in the foreground and keep
the screen awake under the device's power settings, or run the desktop client.
This implementation does **not** install a background service or use GitHub
Actions to simulate the app while it is closed.

Any engine freeze, app suspension or storage error remains visible as a
shortfall in tournament/action counts; AUTO TEST does not fabricate hands.

## v4 experiment: configurable preflop ranges (separate draft branch)

The v3 branch and its overnight run are **not** modified. The v4 draft PR
adds the following independent parameters in **K → AUTO PREFLOP**:

* **Value-only three-bet width:** 5%, 6% (default) or 7% of the 1,326
  physically distinct two-card combinations. Whole hand classes are used:
  * 5%: 99+, AJs+, KQs, AKo = 64/1326 = 4.83%
  * 6%: above plus AQo, KJs = 80/1326 = 6.03%
  * 7%: above plus 88, ATs, KTs = 94/1326 = 7.09%
  The numeric labels are approximate because suit-equivalent classes cannot
  be split without introducing arbitrary suit-dependent decisions. No bluff
  or semi-bluff 3-bets are added to this chart. A separate wider range
  governs **flat calls** against affordable raises, so tightening value
  three-bets does not automatically fold AQo.
* **Short-stack risk control:** tight (-10), balanced (default) or wide
  (+10 percentage points). Before contextual adjustments, the balanced
  **ranked** shove/call ranges target the top ~25% at 10BB, ~40% at 5BB,
  ~62% at 2BB and ~78% at 1BB. The width increases in short-handed
  situations and near the button; it decreases with early position, multiple
  limpers, raises and all-in opponents. The rankings are deterministic
  heuristics, **not** actual ICM-optimal Nash push/fold tables. The chart is
  used BEFORE the generic preflop fold, so a KTo at 0.87BB has a reasoned
  chance to continue rather than hitting an automatic fold wall.

The controller now derives the number and dealer-relative order of **live**
seats from the engine. PokerTH getMyButton()==0 means the player has no
dealer/SB/BB marker, not that the player is in early position. Each
preflop decision records dealer seat, relative seat, living-player count,
big blind, hero's committed chips, pot, chosen three-bet range and
short-stack adjustment in the JSON for later audit.

Regression tests include physical-combination counts (64/80/94),
neighboring hands and 1,326-combo monotonic range widening, exact
Kc-Th at 0.87BB and Ah-As re-raise fold incidents, 2/3/4-player live seat
order, all six pocket-aces combinations, and weak hands kept outside
the emergency push range when sufficient chips remain.

**Known limitations:** no Nash or ICM solver, no action-conditioned opponent
range for the villain, no position-specific *value three-bet* optimization,
and no claim that a fixed 5–7% 3-bet chart is universally optimal. These
are controlled baselines for the overnight tests; do not change settings
during an experiment.
