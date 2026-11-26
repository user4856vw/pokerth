// v6 local-only risk policy. All thresholds are provisional, not GTO.
// Keep the frozen legacy decision available for audit and preserve AA,
// legal free checks, strong made hands and short-stack urgency.
#pragma once
#include "kibitzer_preflop_policy.h"
#include "kibitzer_opponent_model.h"
#include <algorithm>

namespace KibitzerStrategy {
using KibitzerAutoplay::Action;
using KibitzerAutoplay::Decision;
inline Decision fold(const char* why){return {Action::Fold,0,true,why};}
inline Decision call(int price,const char* why){return {price>0?Action::Call:Action::Check,price,true,why};}
inline double odds(int price,int pot){
    return price>0&&pot>0 ? double(price)/(double(price)+pot):1.0;
}
inline double lower(const KibitzerOpponent::Assessment& x) {
    return std::min({x.balanced.equity-1.96*x.balanced.standardError,
        x.valueHeavy.equity-1.96*x.valueHeavy.standardError,
        x.bluffHeavy.equity-1.96*x.bluffHeavy.standardError});
}
inline double upper(const KibitzerOpponent::Assessment& x) {
    return std::max({x.balanced.equity+1.96*x.balanced.standardError,
        x.valueHeavy.equity+1.96*x.valueHeavy.standardError,
        x.bluffHeavy.equity+1.96*x.bluffHeavy.standardError});
}
inline Decision preflop(const KibitzerPreflop::Input& p,Decision old,
                         const KibitzerOpponent::Assessment& model,
                         int opponents,double randomEquity,int eligiblePot,
                         bool sidePotUncertain) {
    if(p.price<=0||p.stack<=0||p.bigBlind<=0||p.price>p.stack)return old;
    const auto c=KibitzerPreflop::cards(p.card0,p.card1);
    if(c.hi==0||(c.pair&&c.hi==14))return old;
    const double required=odds(p.price,eligiblePot);
    const double exposure=double(p.price)/p.stack;
    const double depth=double(p.stack+std::max(0,p.heroCommitted))/p.bigBlind;
    // No chart veto of a very cheap heads-up call closing the action
    // against an all-in opponent. Requires robust range-conditioned equity.
    if(old.action==Action::Fold&&p.alive==2&&opponents==1&&
       p.opponentAllIns==1&&!sidePotUncertain&&model.valid&&
       required<=.10&&exposure<=.10&&lower(model)>required+.015)
        return call(p.price,"v6-hu-cheap-allin-defense-conditioned");
    // Preserve desperate sub-2BB survival; a deep-stack filter must not
    // resurrect the blind-out defect. Multiway side pots remain unmodeled.
    if(depth<=2.0||!p.hasRaise||old.action!=Action::Call)return old;
    const bool strong=KibitzerPreflop::premium(c);
    const double raiseBB=double(p.highestSet)/p.bigBlind;
    if(!strong&&raiseBB>8.0&&required>.12&&depth>10.0)
        return fold("v6-preflop-exceptional-raise-nonpremium");
    if(!strong&&depth>10.0&&opponents>=3&&required>.20&&
       randomEquity+.08<required&&(!model.valid||model.balanced.equity+.05<required))
        return fold("v6-preflop-multiway-price-deficit");
    if(!strong&&!sidePotUncertain&&model.valid&&required>.12&&
       (raiseBB>3.0||exposure>.12||opponents>=3)&&
       upper(model)+.02<required)
        return fold("v6-preflop-conditioned-price-deficit");
    return old;
}
inline Decision postflop(const KibitzerAutoplay::Input& in,Decision old,
                         const KibitzerBoard::Analysis& board,
                         const KibitzerOpponent::Assessment& model,
                         int boardSize,int eligiblePot,bool sidePotUncertain,
                         bool manualRange) {
    if(!board.valid||!in.adviceReady||!in.rangesUsable||in.stack<=0)return old;
    const bool dangerous=board.boardPairs>0||board.maxBoardSuit>=3;
    const bool weak=!board.privateHand;
    // Do not discard a defensible manually-entered range by replacing it
    // with an uncalibrated inferred one. Board-only diagnostics remain logged.
    if(manualRange)return old;
    if(in.price<=0){
        if(old.action==Action::Raise && dangerous && weak)
            return {Action::Check,0,true,"v6-no-board-only-value-probe"};
        if(old.action==Action::Raise && model.valid && lower(model)<.60 &&
           board.madeCategory<4)
            return {Action::Check,0,true,"v6-value-probe-range-not-robust"};
        return old;
    }
    const double required=odds(in.price,eligiblePot);
    const double exposure=double(in.price)/in.stack;
    // For multiway all-in pots we cannot assert a single pot-share EV.
    // Never promote an old fold to a call using that invalid simplification.
    if(sidePotUncertain){
        if(old.action==Action::Call&&weak&&dangerous&&exposure>=.20&&
           board.nominalDrawOuts==0&&model.valid&&upper(model)<required)
            return fold("v6-sidepot-weak-hand-risk-provisional");
        return old;
    }
    if(model.valid){
        const double lo=lower(model),hi=upper(model);
        if(hi<required)return fold("v6-conditioned-equity-below-price");
        // No automatic paid re-raise from a random range. A robust call is
        // preferable to an unsupported aggression candidate.
        if(lo>required+.015)
            return call(in.price,"v6-robust-conditioned-call");
        // On a threatening board, demand stronger evidence when the hand
        // depends on community pairs/kickers or dirty draw outs. The
        // balanced range alone is not a sufficiently robust stack-off.
        if(weak&&dangerous&&exposure>=.18&&required>=.12&&
           (lo<required||model.balanced.equity<required+.04))
            return fold(boardSize==4&&board.nominalDrawOuts>0&&
                        board.unbeatableDrawOuts<board.nominalDrawOuts?
                "v6-paired-board-dirty-draw-risk":"v6-weak-hand-dangerous-board-risk");
        if(weak&&model.aggressiveStreets>=2&&exposure>=.25&&lo<required)
            return fold("v6-repeated-aggression-weak-stack-risk");
        // Avoid resurrecting the v2 river overfold: cheap calls and robust
        // made hands use actual price, not an absolute board-pair veto.
        if(model.balanced.equity>=required)
            return call(in.price,"v6-balanced-conditioned-call-sensitive");
        return fold("v6-balanced-conditioned-fold-sensitive");
    }
    // If history is missing, do not treat unconditioned equity as proof
    // to stake a large fraction of the stack on a board-only hand.
    if(old.action==Action::Call&&weak&&dangerous&&exposure>=.25&&required>=.15)
        return fold("v6-unmodeled-dangerous-board-stack-risk");
    return old;
}
} // namespace KibitzerStrategy
