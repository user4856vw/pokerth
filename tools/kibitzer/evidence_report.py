#!/usr/bin/env python3
"""Offline, reproducible Kibitzer *external reference* audit.

Usage:
  python3 tools/kibitzer/evidence_report.py history.json --session 14
  python3 tools/kibitzer/evidence_report.py history.json --json

Nothing here is an "optimality score". External opening charts omit
opponent adaptation, ICM, and structure differences. Read-only, stdlib-only.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
from typing import Any


def summarize(data: dict[str, Any], *, session: int | None = None,
              version: str | None = "kibitzer-autoplay-v5-shadow") -> dict[str, Any]:
    statuses: Counter[str] = Counter()
    agreement: Counter[str] = Counter()
    grouped: dict[tuple[int, int, str], dict[str, float]] = defaultdict(
        lambda: {"decisions": 0, "externalExpectedRaises": 0.0,
                 "actualRaises": 0, "nearPure": 0, "divergences": 0})
    total = 0
    comparable = 0
    examples: list[dict[str, Any]] = []
    for hand in data.get("hands", []):
        if not isinstance(hand, dict):
            continue
        if session is not None and hand.get("session") != session:
            continue
        for ev in hand.get("entries", []):
            if not isinstance(ev, dict):
                continue
            meta = ev.get("autoplay")
            if not isinstance(meta, dict):
                continue
            if version is not None and meta.get("policyVersion") != version:
                continue
            total += 1
            pre = meta.get("preflopReference")
            benchmark = (pre.get("externalOpeningReference")
                         if isinstance(pre, dict) else None)
            if not isinstance(benchmark, dict):
                statuses["not-logged"] += 1
                continue
            status = benchmark.get("status", "unavailable")
            statuses[status] += 1
            if benchmark.get("applicable") is not True:
                continue
            freq = benchmark.get("openingFrequency")
            if isinstance(freq, bool) or not isinstance(freq, (int, float)):
                statuses["invalid-reference-frequency"] += 1
                continue
            if not 0 <= freq <= 1:
                statuses["invalid-reference-frequency"] += 1
                continue
            comparable += 1
            choice = meta.get("choice")
            is_raise = choice == "BET/RAISE"
            group_key = (pre.get("playersAlive"), benchmark.get("referenceBB"),
                         benchmark.get("referencePosition"))
            group = grouped[group_key]
            group["decisions"] += 1
            group["externalExpectedRaises"] += freq
            group["actualRaises"] += int(is_raise)
            verdict = benchmark.get("policyAgreement", "not-graded")
            agreement[verdict] += 1
            if freq >= .95 or freq <= .05:
                group["nearPure"] += 1
                if verdict == "diverges-from-strong-reference":
                    group["divergences"] += 1
                    examples.append({
                        "session": hand.get("session"), "hand": hand.get("hand"),
                        "cards": ev.get("cards"),
                        "playersAlive": pre.get("playersAlive"),
                        "referenceBB": benchmark.get("referenceBB"),
                        "referencePosition": benchmark.get("referencePosition"),
                        "referenceRaiseFrequency": round(freq, 3),
                        "actualAction": choice,
                        "reason": meta.get("reason"),
                        "referenceSource": benchmark.get("sourceId"),
                    })
    result_groups = []
    for (players, depth, position), g in sorted(
        grouped.items(), key=lambda x: tuple(str(y) for y in x[0])):
        n = g["decisions"]
        result_groups.append({
            "playersAlive": players, "referenceBB": depth, "position": position,
            "decisions": n,
            "referenceMeanRaiseFrequency": round(g["externalExpectedRaises"] / n, 4),
            "observedRaiseFrequency": round(g["actualRaises"] / n, 4),
            "nearPureComparisons": int(g["nearPure"]),
            "divergencesFromNearPure": int(g["divergences"]),
        })
    return {
        "warning": ("REFERENCE COMPARISONS ARE NOT GTO OR MISTAKE VERDICTS. "
                    "Missing, mixed and inapplicable states are not failures."),
        "sessionFilter": session,
        "policyVersionFilter": version,
        "selectedAutoplayDecisions": total,
        "comparableOpeningReferences": comparable,
        "applicabilityStatuses": dict(sorted(statuses.items())),
        "agreements": dict(sorted(agreement.items())),
        "byGameContext": result_groups,
        "nearPureDivergenceExamples": examples[:100],
        "truncatedDivergenceExamples": max(0, len(examples)-100),
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("history", type=Path, help="exported Kibitzer history JSON")
    p.add_argument("--session", type=int, help="only this recorded session")
    p.add_argument("--version", default="kibitzer-autoplay-v5-shadow",
                   help="autoplay policyVersion; use '*' for all versions")
    p.add_argument("--json", action="store_true", help="print machine-readable JSON")
    args = p.parse_args()
    with args.history.open(encoding="utf-8") as fh:
        data = json.load(fh)
    result = summarize(data, session=args.session,
                       version=None if args.version == "*" else args.version)
    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return
    print(result["warning"])
    print(f"Selected decisions: {result['selectedAutoplayDecisions']}")
    print(f"Eligible external comparisons: {result['comparableOpeningReferences']}")
    print("Applicability breakdown:")
    for key, n in result["applicabilityStatuses"].items():
        print(f"  {key}: {n}")
    print("Near-pure reference agreement breakdown:")
    for key, n in result["agreements"].items():
        print(f"  {key}: {n}")
    for row in result["byGameContext"]:
        print(f"  {row['playersAlive']}-handed {row['position']}, "
              f"{row['referenceBB']}BB: n={row['decisions']} "
              f"chart-raise={row['referenceMeanRaiseFrequency']:.1%} "
              f"our-raise={row['observedRaiseFrequency']:.1%} "
              f"near-pure divergences={row['divergencesFromNearPure']}")


if __name__ == "__main__":
    main()
