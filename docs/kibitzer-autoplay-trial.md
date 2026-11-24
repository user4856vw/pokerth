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

Each action's `autoplay` record includes policy version, reason,
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
  has already bet/raised on that river, do not enter a recursive unmodeled
  raise/call war. The current conservative default is fold, marked
  provisional, until opponent-action-conditioned range/EV logic exists.
  It can overfold bluffs and even some strong holdings; this is a safety
  brake, not a solved river strategy.
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
