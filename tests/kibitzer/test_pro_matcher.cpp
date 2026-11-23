#include "kibitzer_pro_matcher.h"
#include <cassert>
#include <cmath>
using namespace KibitzerProHistory;

static Situation example() {
    // Published WSOP hand #1: James Obst folds Td8c preflop in a 5-handed pot.
    Situation s;
    s.street="preflop";s.position="OTHER";s.playersInHand=5;
    s.stackBB=63.88;s.potBB=3.;s.toCallBB=1.;
    assert(setCards(s,"Td8c",""));
    return s;
}
int main() {
    Situation p=example();
    assert(isValid(p));
    auto m=compare(p,p);
    assert(m.eligible && m.close && std::abs(m.score-100)<1e-9);

    Situation altered=p;
    altered.street="flop";assert(!compare(altered,p).eligible);
    altered=p;altered.toCallBB=0;assert(!compare(altered,p).eligible);
    altered=p;altered.playersInHand=2;assert(!compare(altered,p).eligible);
    altered=p;altered.stackBB=4;assert(!compare(altered,p).eligible);
    altered=p;altered.position="BTN";assert(!compare(altered,p).eligible);
    altered=p;altered.toCallBB=2.5;assert(!compare(altered,p).eligible);
    altered=p;assert(setCards(altered,"Ts8s","")); // mismatched suits
    assert(!compare(altered,p).eligible);
    altered=p;assert(setCards(altered,"AdAh","")); // pair versus unpaired
    assert(!compare(altered,p).eligible);
    altered=p;assert(setCards(altered,"2s3h","")); // rank mismatch
    assert(!compare(altered,p).eligible);

    altered=p;altered.stackBB=75;altered.potBB=3.2;altered.toCallBB=1.0;
    m=compare(altered,p);
    assert(m.eligible && m.close && m.score<100);

    Situation flop=p;
    flop.street="flop";flop.position="CO";flop.playersInHand=3;
    flop.stackBB=35;flop.potBB=8;flop.toCallBB=0;
    assert(setCards(flop,"AhKd","Ts9c2h"));
    assert(isValid(flop));
    m=compare(flop,flop);assert(m.eligible && m.close);
    altered=flop;
    assert(setCards(altered,"AhKd","TsTc2h")); // board paired
    assert(!compare(altered,flop).eligible);
    altered=flop;
    assert(setCards(altered,"AhKd","Th9h2h")); // monotone vs rainbow
    assert(!compare(altered,flop).eligible);
    altered=flop;
    assert(setCards(altered,"Ah9d","Ts9c2h")); // board rank hit
    assert(!compare(altered,flop).eligible);

    // Live PokerTH cards use diamonds/hearts/spades/clubs order and ranks
    // within each suit; only relative suits matter to matching.
    Situation live=p;
    int hero[2]={8,19}; // T♦ 8♥ (rank codes 8,6 plus suit stride 13)
    int board[5]={-1,-1,-1,-1,-1};
    assert(fromPokerth(live,hero,board,0));
    assert(isValid(live) && live.ranks[0]==10 && live.ranks[1]==8);
    assert(compare(live,p).close);

    Situation invalid=p;
    assert(!setCards(invalid,"Ad",""));
    assert(!setCards(invalid,"AdKd","3h")); // impossible street length
    invalid=p;invalid.potBB=0;
    assert(!compare(invalid,p).eligible);
    return 0;
}
