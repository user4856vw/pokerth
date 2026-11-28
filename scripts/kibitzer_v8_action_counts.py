#!/usr/bin/env python3
"""Reproduce the captured-action frequency table; never use outcomes/cards."""
import argparse
import collections
import json

def counts(data, sessions):
    out = [[0, 0, 0, 0] for _ in range(9)]
    for h in data["hands"]:
        if h["session"] not in sessions:
            continue
        es = [e for e in h["entries"] if e.get("autoplay", {}).get("strategyP0")]
        if not es:
            continue
        bb = h.get("settlement", {}).get("bigBlind", 0) or next(
            (e["autoplay"].get("preflopReference", {}).get("bigBlind", 0)
             for e in es if e["phase"] == "Preflop"), 0)
        if not bb:
            continue
        # The final recorded hero decision contains the longest public prefix;
        # do not count repeated prefixes from earlier decisions again.
        hist = es[-1]["autoplay"]["strategyP0"]["publicOpponentActions"]
        prior = set()
        for a in hist:
            s, seat, act = a["street"], a["seat"], a["action"]
            price = max(0, a["highestBefore"] - a["committedBefore"])
            ag = act in (4, 5) or (act == 6 and
                   a["committedBefore"] + a["paid"] > a["highestBefore"])
            if s == 0:
                ctx = 0 if a["highestBefore"] <= bb else 2 if seat in prior else 1
            else:
                ctx = 3 + (s - 1) * 2 + int(price > 0)
            if act in range(1, 7):
                z = out[ctx]
                z[3] += 1
                z[2] += act in (3, 4, 5, 6)
                if a["stackBefore"] > price:
                    z[1] += 1
                    z[0] += ag
            if s == 0 and ag:
                prior.add(seat)
    return out

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("export")
    p.add_argument("--sessions", nargs="+", type=int, default=[17, 18])
    a = p.parse_args()
    with open(a.export, encoding="utf-8") as f:
        d = json.load(f)
    print(json.dumps(counts(d, set(a.sessions)), indent=2))
