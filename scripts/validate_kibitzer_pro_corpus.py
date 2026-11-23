#!/usr/bin/env python3
"""Fail-closed manifest check for bundled, source-linked WSOP PHH annotations.

No network requests. Does not certify any observed action as strategically
correct; verifies integrity and provenance of the 11-hand pilot corpus.
"""
import json
import re
from pathlib import Path

CATALOG = Path(__file__).resolve().parents[1] / (
    "src/gui/qt6-qml/resources/verified_wsop23_nlhe.json")
SOURCE = "uoftcprg/phh-dataset"
SOURCE_SHA = "e47fbd5816372360bade4de5d712346fe1bb70f6"
PREFIX = f"https://github.com/{SOURCE}/blob/{SOURCE_SHA}/data/wsop/2023/43/5/"
RANKS = set("23456789TJQKA")
SUITS = set("cdhs")
STREETS = {"preflop": 0, "flop": 3, "turn": 4, "river": 5}
ACTIONS = {"FOLD", "CHECK", "CALL", "BET/RAISE"}
POSITIONS = {"SB", "BB", "BTN", "BTN/SB", "CO", "OTHER"}
FIELDS = ("id", "hand", "player", "hole", "board", "street", "action",
          "position", "playersInHand", "stackBB", "potBB", "toCallBB",
          "amountBB", "sourcePath", "sourceUrl", "actionIndex")


def check_cards(cards: str, count: int) -> bool:
    if not isinstance(cards, str) or len(cards) != 2 * count:
        return False
    pairs = [cards[i:i+2] for i in range(0, len(cards), 2)]
    return len(set(pairs)) == len(pairs) and all(
        p[0] in RANKS and p[1] in SUITS for p in pairs)


def verify(path: Path = CATALOG) -> dict:
    blob = path.read_bytes()
    assert len(blob) < 512_000, "Unexpectedly large bundled pilot dataset"
    doc = json.loads(blob)
    assert doc.get("schemaVersion") == 1
    assert doc.get("catalogId") == "wsop2023-43-day5-nlhe"
    assert doc.get("sourceRevision") == SOURCE_SHA
    assert doc.get("sourceRepository") == f"https://github.com/{SOURCE}"
    assert doc.get("hands") == 11
    rows = doc["decisions"]
    assert len(rows) == 88, "Exact source-transcribed decision count"
    ids = set()
    hand_numbers = set()
    file_paths = set()
    for r in rows:
        assert all(k in r for k in FIELDS)
        assert isinstance(r["id"], str) and r["id"] not in ids
        ids.add(r["id"])
        assert re.fullmatch(r"wsop23-43-h\d+-a\d+", r["id"])
        assert isinstance(r["hand"], int) and 1 <= r["hand"] <= 85
        assert isinstance(r["actionIndex"], int) and r["actionIndex"] >= 1
        assert isinstance(r["player"], str) and 2 <= len(r["player"]) <= 100
        assert r["action"] in ACTIONS
        assert r["street"] in STREETS
        assert r["position"] in POSITIONS
        assert isinstance(r["playersInHand"], int) and 2 <= r["playersInHand"] <= 10
        assert check_cards(r["hole"], 2)
        assert check_cards(r["board"], STREETS[r["street"]])
        assert not (set(r["hole"][i:i+2] for i in (0,2)) &
                    set(r["board"][i:i+2] for i in range(0,len(r["board"]),2)))
        for number in ("stackBB", "potBB", "toCallBB", "amountBB"):
            assert isinstance(r[number], (int,float)) and 0 <= r[number] < 1_000_000
        assert r["potBB"] > 0
        assert r["sourcePath"].startswith("data/wsop/2023/43/5/")
        assert r["sourcePath"].endswith(".phh")
        assert r["sourceUrl"] == (
            f"https://github.com/{SOURCE}/blob/{SOURCE_SHA}/{r['sourcePath']}")
        hand_numbers.add(r["hand"])
        file_paths.add(r["sourcePath"])
    assert len(hand_numbers) == len(file_paths) == 11
    assert hand_numbers == {1,2,3,4,62,63,64,65,66,67,68}
    assert len({r["player"] for r in rows}) >= 4
    return {"source_hands": len(hand_numbers), "source_decisions": len(ids),
            "source_revision": SOURCE_SHA}


if __name__ == "__main__":
    print("Historical pilot manifest valid:", verify())
