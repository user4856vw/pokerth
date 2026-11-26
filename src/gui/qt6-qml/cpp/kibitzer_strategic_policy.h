// Local policy overlay. Keep the legacy choice as a counterfactual.
#pragma once
#include "kibitzer_board_analysis.h"
#include "kibitzer_autoplay_logic.h"
namespace KibitzerStrategic {
using namespace KibitzerAutoplay;
inline Decision choose(const Input& in,const KibitzerBoard::Texture& board,
                       const KibitzerBoard::Estimate& range,int aggressionCount,
                       bool potEligible,const Decision& legacy){
    if(in.price<=0 || !board.ready || !in.adviceReady || !in.rangesUsable)return legacy;
    const bool exposed=in.stack>0 && (10LL*in.price>=in.stack ||
                    (in.pot>0&&4LL*in.price>=in.pot));
    const bool weak=!board.privateMade;
    const bool dangerous=board.paired||board.maxSuit>=3||aggressionCount>=2;
    // Check/call only after conditioning on public betting evidence.
    if(range.ready&&potEligible&&aggressionCount>0){
        const double odds=in.required;
        const double low=range.equity[0],mid=range.equity[1],high=range.equity[2];
        if(high+.015<odds)
            return {Action::Fold,0,true,"v6-conditioned-all-scenarios-negative"};
        if(weak&&exposed&&dangerous&&low<odds+.025)
            return {Action::Fold,0,true,"v6-weak-exposed-board-range-insufficient"};
        if(mid+.015<odds)
            return {Action::Fold,0,true,"v6-conditioned-central-equity-deficit"};
        // Do not replace a legal value raise or special river re-raise guard.
        if(!in.riverFacingCounterraise&&legacy.action!=Action::Raise&&
           low>=odds+.025)
            return {Action::Call,in.price,true,"v6-conditioned-robust-call"};
        if(legacy.action==Action::Raise&&weak&&dangerous)
            return {Action::Call,in.price,true,"v6-weak-board-no-unmodeled-raise"};
        return legacy;
    }
    // Missing/multiway history does not authorize a major call with board-only
    // strength. Allow strong draws and made hands to retain legacy treatment.
    if(weak&&exposed&&dangerous&&board.straightOrFlushOuts<8)
        return {Action::Fold,0,true,"v6-weak-exposed-board-no-conditioned-model"};
    return legacy;
}
}
