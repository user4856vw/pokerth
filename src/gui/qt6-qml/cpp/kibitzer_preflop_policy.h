// Conservative, deterministic preflop baseline for LOCAL autonomous trials.
// This is an intentionally explicit chart, not a solver, GTO proof, or a Council
// recommendation. It separates opening, limped pots, raises and shoves.
// Card encoding in PokerTH is rank (2..A) = code % 13 + 2.
#pragma once
#include "kibitzer_autoplay_logic.h"
#include <algorithm>
#include <cmath>

namespace KibitzerPreflop {
using KibitzerAutoplay::Action;
using KibitzerAutoplay::Decision;

struct Input {
    int card0 = -1, card1 = -1;
    int button = 0;          // 1=BTN, 2=SB, 3=BB; larger = earlier positions
    int alive = 10;
    int bigBlind = 0;
    int highestSet = 0;
    int price = 0;
    int stack = 0;           // remaining cash, before action
    int minRaise = 0;        // amount to spend now, not total target bet
    int maxRaise = 0;
    int limpers = 0;
    int opponentAllIns = 0;
    bool hasRaise = false;
};

enum class Position { Early, Middle, Late, Button, SmallBlind, BigBlind };

inline Position position(int button, int alive) {
    if (button == 1) return Position::Button;
    if (button == 2) return Position::SmallBlind;
    if (button == 3) return Position::BigBlind;
    if (button <= 0 || alive < 2) return Position::Early;
    if (button >= alive - 1) return Position::Late;
    if (button >= alive - 3) return Position::Middle;
    return Position::Early;
}

struct Cards {
    int hi = 0;
    int lo = 0;
    bool suited = false;
    bool pair = false;
};

inline Cards cards(int first, int second) {
    if (first < 0 || second < 0 || first >= 52 || second >= 52 || first == second)
        return {};
    const int a = first % 13 + 2, b = second % 13 + 2;
    return {std::max(a,b), std::min(a,b), first / 13 == second / 13, a == b};
}

inline bool openRange(const Cards &c, Position p) {
    const int h=c.hi, l=c.lo;
    if (h < 2) return false;
    const bool ace = h==14;
    const bool broadway = h>=10 && l>=10;
    if (p==Position::BigBlind) return false; // check a free walk
    if (p==Position::Early)
        return (c.pair && h>=8) || (ace && (c.suited ? l>=11 : l>=12))
            || (c.suited && h==13 && l==12);
    if (p==Position::Middle)
        return (c.pair && h>=6) || (ace && (c.suited ? l>=9 : l>=11))
            || (c.suited && ((h==13 && l>=11) || (h==12 && l>=11) ||
                            (h==11 && l==10)))
            || (!c.suited && h==13 && l==12);
    if (p==Position::Late)
        return (c.pair && h>=2) || (ace && (c.suited || l>=9))
            || (c.suited && ((h==13 && l>=9) || (h==12 && l>=9) ||
                (h==11 && l>=9) || (h==10 && l>=8) ||
                (h==9 && l==8) || (h==8 && l==7) || (h==7 && l==6)))
            || (!c.suited && ((h==13 && l>=10) || (h==12 && l>=10) ||
                             (h==11 && l==10)));
    if (p==Position::Button)
        return (c.pair && h>=2) || (ace && (c.suited || l>=8))
            || (c.suited && ((h==13 && l>=7) || (h==12 && l>=8) ||
                (h==11 && l>=8) || (h==10 && l>=8) ||
                (h==9 && l>=7) || (h==8 && l>=6) ||
                (h==7 && l>=6) || (h==6 && l==5)))
            || (!c.suited && ((h==13 && l>=10) || (h==12 && l>=10) ||
                             (h==11 && l==10)));
    // Small blind: tighter than button, no out-of-position limp as a default.
    return (c.pair && h>=2) || (ace && (c.suited || l>=8))
        || (c.suited && ((h==13 && l>=8) || (h==12 && l>=9) ||
            (h==11 && l>=9) || (h==10 && l>=9) || (h==9 && l==8)))
        || (!c.suited && ((h==13 && l>=10) || (h==12 && l>=10)));
}

inline bool strong(const Cards &c) {
    return (c.pair && c.hi>=11) || (c.hi==14 && c.lo==13) ||
        (c.hi==14 && c.lo==12 && c.suited);
}
inline bool premium(const Cards &c) {
    return (c.pair && c.hi>=12) || (c.hi==14 && c.lo==13);
}

// size is the incremental amount passed to GameHandler::raise(), which
// internally adds to hero's current bet. Never request an illegal raise.
inline Decision sizedRaise(const Input &in, int desired, const char *why) {
    if (in.minRaise<=0 || in.maxRaise<=0 || in.maxRaise<in.minRaise)
        return {in.price==0 ? Action::Check : Action::Fold,0,true,
                "preflop-raise-unavailable"};
    const int size=std::max(in.minRaise,desired);
    if (size>in.maxRaise)
        return {in.price==0 ? Action::Check : Action::Fold,0,true,
                "preflop-raise-too-expensive"};
    return {Action::Raise,size,true,why};
}

inline Decision choose(const Input &in) {
    const Cards c=cards(in.card0,in.card1);
    if (c.hi==0 || in.bigBlind<=0 || in.stack<=0 || in.alive<2 ||
        in.price<0 || in.price>in.stack || in.highestSet<0)
        return {in.price==0?Action::Check:Action::Fold,0,true,
                "preflop-invalid-context"};
    const Position p=position(in.button,in.alive);
    const bool canOpen = openRange(c,p);
    const double stackBB=double(in.stack+std::max(0,in.highestSet-in.price))/
                         double(in.bigBlind);
    const bool multiwayShove=in.opponentAllIns>1;

    // Do not use uniform-random multiway showdown equity to call a shove.
    // A ten-player 22 preflop stack-off is NOT a 1-on-1 pot-odds decision.
    if (in.opponentAllIns>0) {
        if (multiwayShove || in.price>in.stack/3)
            return {(c.pair && c.hi>=13) ? Action::Call : Action::Fold,
                    (c.pair && c.hi>=13) ? in.price : 0,true,
                    "preflop-shove-multiway-tight-no-icm"};
        return {premium(c)?Action::Call:Action::Fold,
                premium(c)?in.price:0,true,
                "preflop-shove-tight-no-icm"};
    }

    if (in.hasRaise) {
        // Unknown opponent ranges: continue only with credible value hands.
        // No automatic 3-bet based on equity versus uniformly random hands.
        if (premium(c) && in.price<=in.stack/3 && stackBB>12.0) {
            const int target=in.price + 2*std::max(in.bigBlind,in.highestSet);
            return sizedRaise(in,target,"preflop-value-3bet-provisional");
        }
        const bool defend = strong(c) || (c.pair && c.hi>=9 && in.price<=in.stack/10);
        if (defend && in.price<=in.stack/5)
            return {Action::Call,in.price,true,"preflop-raise-defend-provisional"};
        return {Action::Fold,0,true,"preflop-raise-fold-outside-range"};
    }

    // A checked big blind must remain a check; opening makes no sense here.
    if (in.price==0 && p==Position::BigBlind)
        return {Action::Check,0,true,"preflop-big-blind-free-check"};

    // Isolate with value hands, otherwise avoid over-limping weak holdings.
    if (in.limpers>0) {
        const bool iso=strong(c) || (canOpen && c.pair && c.hi>=8);
        if (iso && stackBB>12.0) {
            const int target=in.price +
                (5+2*std::min(in.limpers,4))*in.bigBlind/2;
            return sizedRaise(in,target,"preflop-limper-value-isolation");
        }
        if (in.price==0) return {Action::Check,0,true,"preflop-limp-free-check"};
        const bool overlimp = in.price<=in.bigBlind &&
            ((c.pair && c.hi>=2) || (c.suited && c.hi-c.lo==1 && c.hi>=6));
        return {overlimp?Action::Call:Action::Fold,
                overlimp?in.price:0,true,
                "preflop-limp-no-random-multiway-equity"};
    }

    if (!canOpen)
        return {in.price==0?Action::Check:Action::Fold,0,true,
                "preflop-unopened-outside-position-range"};

    if (stackBB<=12.0) {
        // Deliberately conservative experimental push/fold regime.
        if (strong(c) || (p==Position::Button && c.pair && c.hi>=7))
            return sizedRaise(in,in.maxRaise,"preflop-short-stack-shove-provisional");
        return {in.price==0?Action::Check:Action::Fold,0,true,
                "preflop-short-stack-conservative-fold"};
    }
    const int target=in.price + in.bigBlind*3/2; // total ~2.5 BB
    return sizedRaise(in,target,"preflop-position-open-2p5bb-provisional");
}

} // namespace KibitzerPreflop
