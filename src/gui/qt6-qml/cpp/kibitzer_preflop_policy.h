// Conservative, deterministic preflop baseline for LOCAL autonomous trials.
// This is an intentionally explicit chart, not a solver, GTO proof, or a Council
// recommendation. It separates opening, limped pots, raises and shoves.
// Card encoding in PokerTH is rank (2..A) = code % 13 + 2.
#pragma once
#include "kibitzer_autoplay_logic.h"
#include "kibitzer_preflop_ranges.h"
#include <algorithm>
#include <cmath>

namespace KibitzerPreflop {
using KibitzerAutoplay::Action;
using KibitzerAutoplay::Decision;

struct Input {
    int card0 = -1, card1 = -1;
    int button = 0;          // 1=BTN, 2=SB, 3=BB; 0=unmarked, NOT UTG
    int roleIndex = -1;      // actual dealer-relative seat index (0 BTN, 1 SB, 2 BB)
    int alive = 10;
    int bigBlind = 0;
    int highestSet = 0;
    int price = 0;
    int stack = 0;           // remaining cash, before action
    int heroCommitted = 0;   // chips posted on this betting street
    int pot = 0;             // pot before a call, including opponents' bets
    int valueThreeBetPercent = 6; // allowed 5, 6, 7; defaults to ~6%
    int shortStackAdjustment = 0; // percentage points: -10, 0, +10
    int minRaise = 0;        // amount to spend now, not total target bet
    int maxRaise = 0;
    int limpers = 0;
    int opponentAllIns = 0;
    bool hasRaise = false;
};

enum class Position { Early, Middle, Late, Button, SmallBlind, BigBlind };

inline Position position(int button, int alive,int roleIndex=-1) {
    // Use live seats clockwise from the DEALER. PokerTH's getMyButton()==0
    // for every unmarked seat, so it is not an ordinal table position.
    if (roleIndex>=0 && roleIndex<alive) {
        if (roleIndex==0) return Position::Button; // BTN/SB heads-up
        if (alive==2 && roleIndex==1) return Position::BigBlind;
        if (roleIndex==1) return Position::SmallBlind;
        if (roleIndex==2) return Position::BigBlind;
        if (roleIndex>=alive-2) return Position::Late;
        if (roleIndex>=alive-4) return Position::Middle;
        return Position::Early;
    }
    if (button==1) return Position::Button;
    if (button==2) return Position::SmallBlind;
    if (button==3) return Position::BigBlind;
    return Position::Early; // conservative only if dealer marker is missing
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
    const Position p=position(in.button,in.alive,in.roleIndex);
    const bool canOpen = openRange(c,p);
    const double stackBB=double(in.stack+std::max(0,in.heroCommitted))/
                         double(in.bigBlind);
    const bool multiwayShove=in.opponentAllIns>1;

    // Absolute pocket aces are a special case in the ordinary chip-EV
    // local tournament. Facing one or several all-ins, call rather than
    // incorrectly applying the tight "unknown villain" fold branch.
    // If a legal value raise exists, make it; otherwise call, never fold.
    // Satellite bubble / ICM exceptions are NOT modeled in this baseline.
    if (c.pair && c.hi==14) {
        if (in.opponentAllIns>0)
            return {in.price==0?Action::Check:Action::Call,in.price,true,
                    "preflop-aa-allin-continue-no-icm"};
        if (in.minRaise>0 && in.maxRaise>=in.minRaise) {
            int desired=in.price + in.bigBlind*3/2;
            if (in.hasRaise)
                desired=in.price + 2*std::max(in.bigBlind,in.highestSet);
            else if (in.limpers>0)
                desired=in.price + (4+std::min(in.limpers,4))*in.bigBlind;
            if (stackBB<=12.0)
                desired=in.maxRaise;
            const int size=std::max(in.minRaise,std::min(desired,in.maxRaise));
            return {Action::Raise,size,true,"preflop-aa-value-raise-no-icm"};
        }
        return {in.price==0?Action::Check:Action::Call,in.price,true,
                "preflop-aa-legal-raise-unavailable-call"};
    }

    // Short-stack urgency is considered BEFORE the ordinary raise/limp/open
    // chart. Under 10BB, folding until blind-out is usually a larger error
    // than taking a modestly imperfect shove/call. Opponent ranges and ICM
    // are not modeled here; every decision is provisional.
    if (stackBB<=10.0) {
        int width=KibitzerPreflopRanges::shortStackWidth(
            stackBB,in.alive,in.roleIndex,in.shortStackAdjustment,
            in.hasRaise,in.opponentAllIns,in.limpers);
        // An almost-free last-blind call can be wider, but never against
        // multiple all-in opponents without a side-pot model.
        if (in.price>0 && in.pot>0 && in.opponentAllIns<2 &&
            static_cast<long long>(in.price)*5 <= in.pot)
            width=std::min(98,width+12);
        const bool continueHand=KibitzerPreflopRanges::topPercent(
            KibitzerPreflopRanges::decode(in.card0,in.card1),width);
        if (in.price==0 && p==Position::BigBlind)
            return {Action::Check,0,true,"preflop-short-stack-free-walk"};
        if (!continueHand)
            return {in.price==0?Action::Check:Action::Fold,0,true,
                    "preflop-short-stack-range-fold"};
        if (in.hasRaise || in.opponentAllIns>0) {
            return {in.price==0?Action::Check:Action::Call,in.price,true,
                    "preflop-short-stack-priced-continue"};
        }
        // Try to shove to maximize fold equity. If an all-in raise is
        // unavailable, still call a priced blind rather than erroneously fold.
        if (in.maxRaise>0 && in.minRaise>0 && in.maxRaise>=in.minRaise)
            return {Action::Raise,in.maxRaise,true,
                    "preflop-short-stack-shove-ranked-range"};
        return {in.price==0?Action::Check:Action::Call,in.price,true,
                "preflop-short-stack-call-raise-unavailable"};
    }

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
        const bool chart=KibitzerPreflopRanges::valueThreeBet(
            KibitzerPreflopRanges::decode(in.card0,in.card1),
            in.valueThreeBetPercent);
        if (chart && in.price<=in.stack/4) {
            const int target=in.price+2*std::max(in.bigBlind,in.highestSet);
            const int size=std::max(in.minRaise,target);
            // Value 3-bets are not unrestricted stack commitments. When
            // a normal legal 3-bet costs over half the stack, flat-call
            // strong hands rather than manufacture an involuntary shove.
            if (in.minRaise>0 && in.maxRaise>=in.minRaise &&
                size<=in.maxRaise && size<=in.stack/2)
                return {Action::Raise,size,true,
                        "preflop-configured-value-three-bet"};
        }
        const bool defend = chart || strong(c) ||
            (c.pair && c.hi>=9 && in.price<=in.stack/10);
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

    // 10–12BB also benefits from a tighter push/fold chart, not a blanket fold.
    if (stackBB<=12.0 && strong(c))
        return sizedRaise(in,in.maxRaise,"preflop-twelvebb-value-push");
    const int target=in.price + in.bigBlind*3/2; // total ~2.5 BB
    return sizedRaise(in,target,"preflop-position-open-2p5bb-provisional");
}

} // namespace KibitzerPreflop
