// v6 local-only risk policy. All thresholds are provisional, not GTO.
// Keep the frozen legacy decision available for audit and preserve AA,
// legal free checks, strong made hands and short-stack urgency.
#pragma once
#include "kibitzer_preflop_policy.h"
#include "kibitzer_opponent_model.h"
#include <algorithm>
#include <limits>
#include <vector>

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

// V7: conservative showdown-share lower bound for a preflop call.
// Verify the ledger against the engine pot; if it does not reconcile,
// do not use it to authorize a multiway stack commitment.
// Each opponent's contribution beyond the hero's final contribution is
// excluded. Hero's global showdown share cannot exceed hero's share in
// any pot tier for which hero is eligible. This is a lower bound assuming
// all remaining players reach showdown, NOT a full future-betting/ICM EV.
struct PreflopPotBound {
    bool valid=false;
    int eligible=0; // chips already in the pot, before adding the call
};
inline PreflopPotBound preflopPotBound(int pot,int committed,int price,
                                      const std::vector<int>& others) {
    if(pot<=0||committed<0||price<=0||others.empty())return {};
    const long long cap=static_cast<long long>(committed)+price;
    long long total=committed,eligible=committed;
    bool matched=false;
    for(int x:others){
        if(x<0)return {};
        total+=x;
        eligible+=std::min<long long>(x,cap);
        matched=matched||x>=cap;
    }
    if(!matched||total!=pot||eligible<=0||eligible>pot)return {};
    return {true,static_cast<int>(eligible)};
}
inline bool usableEstimate(const KibitzerOpponent::Estimate& e){
    return e.valid&&std::isfinite(e.equity)&&e.equity>=0&&e.equity<=1&&
        std::isfinite(e.standardError)&&e.standardError>=0;
}
inline bool usableAssessment(const KibitzerOpponent::Assessment& m){
    return m.valid&&usableEstimate(m.balanced)&&usableEstimate(m.valueHeavy)&&
        usableEstimate(m.bluffHeavy);
}
// Count every public preflop price increase, including short all-in increases.
// A short all-in does not necessarily reopen raising; the engine's legal
// action bounds govern that. We never use it to authorize another raise.
inline int preflopPriceIncreases(const std::vector<KibitzerOpponent::Observation>& history){
    int n=0;
    for(const auto& a:history)
        if(a.street==0&&KibitzerOpponent::aggression(a)&&
           static_cast<long long>(a.committedBefore)+a.paid>a.highestBefore)
            ++n;
    return n;
}
// Exact break-even fold frequency for a supplied single-caller hypothesis.
// P includes existing commitments, risk and response are additional chips.
// The comparison is to surrendering the pot, not to checking/calling.
inline double shoveBreakEvenFold(int pot,int risk,int response,double equity){
    if(pot<=0||risk<=0||response<0||!std::isfinite(equity)||equity<0||equity>1)
        return std::numeric_limits<double>::quiet_NaN();
    const double called=equity*(double(pot)+risk+response)-risk;
    return called>=0?0.0:-called/(pot-called);
}
inline Decision preflopV7(const KibitzerPreflop::Input& p,Decision old,
                          const KibitzerOpponent::Assessment& model,int opponents,
                          double randomEquity,int eligiblePot,bool sidePotUncertain,
                          int priceIncreases,PreflopPotBound bound) {
    const auto fallback=std::isfinite(randomEquity)&&randomEquity>=0&&randomEquity<=1?
        preflop(p,old,model,opponents,randomEquity,eligiblePot,sidePotUncertain):old;
    if(p.price<=0||p.price>p.stack||p.bigBlind<=0||p.stack<=0)return fallback;
    const auto c=KibitzerPreflop::cards(p.card0,p.card1);
    if(c.hi==0||(c.pair&&c.hi==14))return fallback; // preserve the existing AA invariant
    const bool laterRaise=p.heroPreviouslyRaised||priceIncreases>=2;
    const bool contested=laterRaise||p.opponentAllIns>0||
        (p.hasRaise&&fallback.action==Action::Call);
    if(!contested)return fallback;
    const int pot=sidePotUncertain?(bound.valid?bound.eligible:0):eligiblePot;
    // Reconsider BOTH old calls and old folds. The old stack/3 and stack/5
    // cutoffs must not veto positive model-supported showdown chip EV.
    // The existing 1.96-SE envelope is only a sampling-uncertainty check
    // across three provisional priors, not a calibrated confidence interval
    // for the opponent's true range or a guarantee of future realization.
    if(pot>0&&opponents>0&&usableAssessment(model)){
        const double required=odds(p.price,pot);
        if(lower(model)>required)
            return call(p.price,sidePotUncertain?
                "v7-preflop-sidepot-conservative-showdown-call":
                "v7-preflop-conditioned-showdown-call");
        // Upper equity against all opponents does NOT bound equity against
        // a subset eligible for a side pot. Never use it to veto such a call.
        if(!sidePotUncertain&&upper(model)<required)
            return fold("v7-preflop-conditioned-showdown-deficit");
    }
    // A three-bet chart is not authority for a fourth or later bet.
    // No reliable response model: do not manufacture a further raise.
    if(laterRaise&&fallback.action==Action::Raise)
        return fold("v7-preflop-later-raise-no-supported-call-or-reraise");
    // Preserve the frozen fallback in sensitive/missing-model cases.
    // The audit retains its original reason; this is not a new EV claim.
    return fallback;
}
inline Decision postflopV7(const KibitzerAutoplay::Input& in,Decision old,
                           const KibitzerBoard::Analysis& board,
                           const KibitzerOpponent::Assessment& model,
                           int boardSize,int eligiblePot,bool sidePotUncertain,
                           bool manualRange) {
    // A free first action with high overall equity is not evidence that
    // worse hands will call a bet. Remove unmodeled value probes on the
    // terminal street and boards permitting trips/full houses or flushes.
    // A genuinely modeled raise is nonprovisional in the legacy policy.
    if(in.price==0&&old.action==Action::Raise&&old.provisional&&board.valid&&
       (boardSize==5||board.boardPairs>0||board.maxBoardSuit>=3))
        return {Action::Check,0,true,"v7-no-unmodeled-value-probe-on-river-or-threat-board"};
    return postflop(in,old,board,model,boardSize,eligiblePot,sidePotUncertain,manualRange);
}
} // namespace KibitzerStrategy
