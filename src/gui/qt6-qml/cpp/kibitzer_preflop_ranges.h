// Transparent, adjustable local-only preflop reference ranges.
// These are NOT solver-derived GTO charts; all automatic decisions remain
// provisional. Percentages count physical two-card combinations (1,326),
// not 169 equally weighted starting-hand classes.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace KibitzerPreflopRanges {
struct Hand {
    int hi=0,lo=0;
    bool suited=false,pair=false;
};
inline Hand decode(int a,int b) {
    if (a<0||b<0||a>=52||b>=52||a==b) return {};
    const int x=a%13+2,y=b%13+2;
    return {std::max(x,y),std::min(x,y),a/13==b/13,x==y};
}
inline bool classMatches(const Hand &h,int hi,int lo,int suited) {
    return h.hi==hi&&h.lo==lo&&
        (suited<0||(suited==1&&h.suited)||(suited==0&&!h.suited));
}
// Five-percent chart: 99+, AJs+, KQs, AKo = 64 / 1326 = 4.83%.
// Six-percent chart: plus KJs and AQo = 80 / 1326 = 6.03%.
// Seven-percent chart: plus 88, ATs, KTs = 94 / 1326 = 7.09%.
// Full hand classes, no arbitrary exclusions of suits.
inline bool valueThreeBet(const Hand &h,int percent=6) {
    if (h.hi<2||percent<5||percent>7) return false;
    if (h.pair && h.hi>=9) return true;
    if (h.suited &&
        ((h.hi==14 && h.lo>=11) || classMatches(h,13,12,1)))
        return true;
    if (classMatches(h,14,13,0)) return true;
    if (percent>=6 && (classMatches(h,14,12,0) ||
                       classMatches(h,13,11,1))) return true;
    if (percent>=7 && ((h.pair&&h.hi==8) ||
        classMatches(h,14,10,1) || classMatches(h,13,10,1)))
        return true;
    return false;
}

// A reproducible *ranking approximation* for short-stack push/fold.
// No claim that the numeric rank score is actual showdown equity or ICM.
inline double shortStackScore(const Hand &h) {
    if (h.hi<2) return -1e9;
    if (h.pair) return 60.0 + h.hi*2.7; // all pairs remain relatively strong
    const int gap=h.hi-h.lo-1;
    const double gapBonus=gap==0?4.0:gap==1?2.0:gap==2?0.0:
                          gap==3?-2.0:-4.0;
    return h.hi*3.1 + h.lo*1.55 + (h.suited?5.0:0.0) +
        gapBonus + (h.hi==14?2.0:0.0);
}
struct RankedClass {
    Hand hand;
    double score=0.0;
    int combos=0;
};
inline const std::vector<RankedClass>& rankedClasses() {
    static const auto table=[] {
        std::vector<RankedClass> classes;
        classes.reserve(169);
        for (int hi=2;hi<=14;++hi) {
            classes.push_back({{hi,hi,false,true},
                shortStackScore({hi,hi,false,true}),6});
            for (int lo=2;lo<hi;++lo) {
                classes.push_back({{hi,lo,true,false},
                    shortStackScore({hi,lo,true,false}),4});
                classes.push_back({{hi,lo,false,false},
                    shortStackScore({hi,lo,false,false}),12});
            }
        }
        std::stable_sort(classes.begin(),classes.end(),
            [](const RankedClass& a,const RankedClass& b) {
                if (a.score!=b.score) return a.score>b.score;
                if (a.hand.hi!=b.hand.hi) return a.hand.hi>b.hand.hi;
                if (a.hand.lo!=b.hand.lo) return a.hand.lo>b.hand.lo;
                return a.hand.suited;
            });
        return classes;
    }();
    return table;
}
inline int comboCountInTopPercent(int pct) {
    if (pct<=0) return 0;
    if (pct>=100) return 1326;
    int used=0;
    for (const RankedClass &c:rankedClasses()) {
        if (double(used+c.combos/2.0)*100.0>double(pct)*1326.0)
            break;
        used+=c.combos;
    }
    return used;
}
inline bool topPercent(const Hand &h,int pct) {
    if (h.hi<2||pct<=0) return false;
    if (pct>=100) return true;
    int used=0;
    for (const RankedClass &c:rankedClasses()) {
        if (double(used+c.combos/2.0)*100.0>double(pct)*1326.0)
            return false;
        if (c.hand.hi==h.hi&&c.hand.lo==h.lo&&
            c.hand.pair==h.pair&&(h.pair||c.hand.suited==h.suited))
            return true;
        used+=c.combos;
    }
    return false;
}

// Baseline: 25% at 10BB, 40% at 5BB, 62% at 2BB, 78% at 1BB.
// All widths refer to the 1,326-combination ranking above, not 169 hands.
// Tight/normal/wide controls shift each threshold by +/-10 points.
inline int shortStackWidth(double stackBB,int alive,int positionIndex,
                           int adjustment=0,bool facingRaise=false,
                           int allIns=0,int limpers=0) {
    if (!(stackBB>0.0)||stackBB>10.0) return 0;
    int pct=stackBB<=1.0?78:stackBB<=2.0?62:
            stackBB<=5.0?40:25;
    if (alive<=3) pct+=14;
    else if (alive<=5) pct+=8;
    if (positionIndex==0) pct+=5; // button
    else if (positionIndex==1&&alive>2) pct+=2; // small blind
    else if (positionIndex>=3&&alive>=7&&
             positionIndex<alive-3) pct-=7; // early seats
    pct+=std::max(-10,std::min(10,adjustment));
    if (facingRaise) pct-=13;
    if (limpers>=2) pct-=5;
    if (allIns==1) pct-=8;
    if (allIns>=2) pct-=25;
    return std::max(4,std::min(98,pct));
}
} // namespace KibitzerPreflopRanges
