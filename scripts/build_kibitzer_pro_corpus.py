#!/usr/bin/env python3
"""Regenerate the reviewed 2023 WSOP no-limit pilot corpus from the PHH files.

Usage:
    git clone https://github.com/uoftcprg/phh-dataset /tmp/phh-dataset
    git -C /tmp/phh-dataset checkout e47fbd5816372360bade4de5d712346fe1bb70f6
    python3 scripts/build_kibitzer_pro_corpus.py /tmp/phh-dataset [--check]

Requires Python 3.11+ (tomllib). Does not scrape poker sites or use any private
hand data. All other poker variants are excluded. Reuses PHH *facts* with
explicit attribution and an exact link back to the original transcript.
"""
import argparse
import json
import math
import tomllib
from pathlib import Path

SOURCE_SHA="e47fbd5816372360bade4de5d712346fe1bb70f6"
SOURCE_REPO="uoftcprg/phh-dataset"
NAMES=["00-02-07","00-08-38","00-15-36","00-18-39",
       "02-51-10","02-53-09","02-54-12","02-56-12",
       "02-57-27","03-00-32","03-02-41"]


def annotate(raw: dict, source_path: str) -> list[dict]:
    assert raw["variant"] == "NT", f"Not NLHE: {source_path}"
    players=raw["players"]
    stacks=raw["starting_stacks"]
    ante=raw["antes"]
    blinds=raw["blinds_or_straddles"]
    big_blind=raw["min_bet"]
    n=len(players)
    assert n==len(stacks)==len(ante)==len(blinds) and big_blind>0
    spent=[ante[i]+blinds[i] for i in range(n)]
    street_bet=list(blinds)
    folded=[False]*n
    hole=[None]*n
    board=""
    street="preflop"
    result=[]
    for ix,action in enumerate(raw["actions"],1):
        words=action.split("#",1)[0].strip().split()
        if not words:continue
        if words[:2]==["d","dh"]:
            assert len(words)==4 and words[2].startswith("p")
            seat=int(words[2][1:])-1
            assert 0<=seat<n
            hole[seat]=words[3]
            continue
        if words[:2]==["d","db"]:
            assert len(words)==3
            board+=words[2]
            street={6:"flop",8:"turn",10:"river"}[len(board)]
            street_bet=[0]*n
            continue
        if len(words)>=2 and words[0].startswith("p") and words[1]=="sm":
            continue
        assert len(words) in (2,3), f"Unsupported PHH action: {action}"
        seat=int(words[0][1:])-1
        kind=words[1]
        assert 0<=seat<n and kind in ("f","cc","cbr") and not folded[seat]
        assert hole[seat] and len(hole[seat])==4
        highest=max(street_bet)
        price=max(0,highest-street_bet[seat])
        pot=sum(spent)
        action_name=("FOLD" if kind=="f" else "CHECK" if kind=="cc"
                     and price==0 else "CALL" if kind=="cc" else "BET/RAISE")
        amount=0 if kind=="f" else price if kind=="cc" else int(words[2])-street_bet[seat]
        assert amount>=0 and pot>0
        role=("BTN/SB" if n==2 else "SB") if seat==0 else (
            "BB" if seat==1 else "BTN" if seat==n-1 else
            "CO" if seat==n-2 else "OTHER")
        def bb(value):return float(f"{value/big_blind:.2f}")
        record={
            "id": f"wsop23-43-h{raw['hand']}-a{ix}",
            "hand": raw["hand"],
            "player": players[seat],
            "hole": hole[seat],
            "board": board,
            "street": street,
            "action": action_name,
            "position": role,
            "playersInHand": sum(not f for f in folded),
            "stackBB": bb(stacks[seat]-spent[seat]),
            "potBB": bb(pot),
            "toCallBB": bb(price),
            "amountBB": bb(amount),
            "sourcePath": source_path,
            "sourceUrl": (f"https://github.com/{SOURCE_REPO}/blob/"
                          f"{SOURCE_SHA}/{source_path}"),
            "actionIndex": ix,
        }
        result.append(record)
        if kind=="f":
            folded[seat]=True
        elif kind=="cc":
            street_bet[seat]+=price
            spent[seat]+=price
        else:
            street_bet[seat]=int(words[2])
            spent[seat]+=amount
        assert spent[seat]<=stacks[seat]
    return result


def generate(source_root: Path) -> dict:
    rows=[]
    for name in NAMES:
        path=f"data/wsop/2023/43/5/{name}.phh"
        phh=tomllib.loads((source_root/path).read_text(encoding="utf-8"))
        rows.extend(annotate(phh,path))
    assert len(rows)==88 and len({r['hand'] for r in rows})==11
    return {"schemaVersion":1,
        "catalogId":"wsop2023-43-day5-nlhe",
        "source":"Universal, Open, Free, and Transparent Computer Poker Research Group / PHH dataset",
        "sourceRepository":f"https://github.com/{SOURCE_REPO}",
        "sourceRevision":SOURCE_SHA,
        "sourceLicense":"MIT — dataset repository; original broadcast rights not conveyed",
        "event":"2023 WSOP Event #43: $50,000 Poker Players Championship, Day 5 (televised final table)",
        "variant":"No-limit Texas Hold'em only (PHH NT)",
        "coverageNote":"11 complete NT source hands from mixed-game broadcast. Not representative of elite-player frequencies; selected final-table context.",
        "hands":11,
        "decisions":rows}


def main():
    p=argparse.ArgumentParser()
    p.add_argument("source_root",type=Path)
    p.add_argument("--check",action="store_true",help="compare regenerated facts with bundled version")
    a=p.parse_args()
    doc=generate(a.source_root)
    target=Path(__file__).resolve().parents[1] / (
        "src/gui/qt6-qml/resources/verified_wsop23_nlhe.json")
    if not a.check:
        target.write_text(json.dumps(doc,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print("Wrote",len(doc["decisions"]),"real decisions to",target)
        return
    old=json.loads(target.read_text(encoding="utf-8"))
    assert old.keys()==doc.keys()
    assert len(old["decisions"])==len(doc["decisions"])
    for given,new in zip(old["decisions"],doc["decisions"]):
        assert given.keys()==new.keys(),given.get("id")
        for key in given:
            if isinstance(given[key],float):
                assert math.isclose(given[key],new[key],abs_tol=.011), (given["id"],key)
            else:
                assert given[key]==new[key],(given["id"],key,given[key],new[key])
    print("Regeneration verified:",len(doc["decisions"]),"observed decisions")


if __name__=="__main__":
    main()
