# Kibitzer AUTO TEST: an auditable local self-play experiment

AUTO TEST is deliberately **opt-in** on the local game table. Start a local
tournament with the desired settings, then tap **AUTO** beside the K button.
Tap **STOP** to return to human play. It cannot act in a network game.

The app plays every hero decision automatically, advances paused hands and
starts a fresh local tournament after an elimination/win. The run stops after
eight hours (wall clock) or 100 completed tournaments, whichever happens first.
It does not restart itself after the app is closed.

Each actual action is recorded in the existing private Kibitzer JSON history.
The extra `autoplay` field includes the policy version, action reason, whether
an inference/fallback was required, Council headline/stance, and the
contemporaneous recommendation. The original Monte Carlo snapshot is reused
by the recorder to avoid a second, different random calculation at action time.
Use **K → History / Progress → Copy backup** to export the results. Existing
archive retention limits still apply (up to 2,500 settled hands / 1,000 finishes):
export regularly rather than assuming unlimited month-long retention.

## Exact decision mapping

* An explicitly modeled BET EV raise edge with a **legal** best size → raise.
  This is conditional on the manually supplied opponent response ranges.
* Otherwise, free check → check; favorable call beyond the comparison margin
  → call; unfavorable call beyond it → fold. These are **provisional** if
  Council has not reached that explicit combined stance. A single pot-odds
  lens passing is not silently promoted into Council consensus.
* If Council is undecided, the direct equity-versus-price comparison breaks the
  tie. If the model is unavailable or an assigned range cannot be evaluated,
  the safe fallback is check when free, otherwise fold.
* A Stage A `STUDY A RAISE` indication does **not** specify a bet size or
  opponent response model. For this local **experiment only**, when there is
  no contradictory combined Council stance, the controller tries the legal
  minimum raise. This extra sizing assumption is flagged `provisional`.
  Any explicit Council passive recommendation takes precedence; the
  experimental raise is never presented as a modeled positive-EV finding.

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
