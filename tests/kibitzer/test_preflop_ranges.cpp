#include "kibitzer_preflop_ranges.h"
#include <cassert>
#include <iostream>
using namespace KibitzerPreflopRanges;
static bool is3(int hi,int lo,bool suited,int pct=6) {
    return valueThreeBet({hi,lo,suited,hi==lo},pct);
}
int main() {
    // Exact weighted physical-combination counts, not 169 class counts.
    // 5/6/7 percent tiers correspond to 64/80/94 of 1,326 deals.
    for (int pct=5;pct<=7;++pct) {
        int hands=0;
        for (int a=0;a<52;++a) for(int b=a+1;b<52;++b)
            if(valueThreeBet(decode(a,b),pct)) ++hands;
        assert(hands==(pct==5?64:pct==6?80:94));
    }
    assert(is3(14,14,false)); // all 6 AA combos
    assert(is3(13,13,false)); // KK
    assert(is3(12,12,false)); // QQ
    assert(is3(9,9,false));   // 99 (baseline)
    assert(!is3(8,8,false,5)); assert(is3(8,8,false,7));
    assert(is3(14,13,false)); // AKo (12 combos)
    assert(is3(14,11,true));  // AJs
    assert(is3(13,12,true));  // KQs
    assert(!is3(14,12,false,5)); assert(is3(14,12,false,6)); // AQo
    assert(!is3(13,11,true,5)); assert(is3(13,11,true,6)); // KJs
    assert(!is3(14,10,true,6)); assert(is3(14,10,true,7)); // ATs
    assert(!is3(13,10,true,6)); assert(is3(13,10,true,7)); // KTs
    assert(!is3(14,10,false,7)); // ATo not an unpriced value 3-bet
    assert(!is3(13,12,false,7)); // KQo not automatically 3-bet
    assert(!is3(7,2,false,7));  // 72o outside every chart
    assert(!valueThreeBet(decode(0,0),6));
    assert(!valueThreeBet(decode(12,38),4));
    assert(!valueThreeBet(decode(12,38),8));
    assert(valueThreeBet(decode(25,38),6));

    // Monotonic short-stack widths. Each new tier adds classes; it cannot
    // turn an already-playable hand into a fold as the stack gets smaller.
    for (int i=0;i<3;++i) {
        const int alive=i==0?10:i==1?5:2;
        const int pos=i==0?3:0;
        const int w10=shortStackWidth(10,alive,pos);
        const int w5=shortStackWidth(5,alive,pos);
        const int w2=shortStackWidth(2,alive,pos);
        const int w1=shortStackWidth(1,alive,pos);
        assert(w10 < w5 && w5 < w2 && w2 < w1);
        for (int a=0;a<52;++a) for(int b=a+1;b<52;++b) {
            const Hand h=decode(a,b);
            if (topPercent(h,w10)) assert(topPercent(h,w5));
            if (topPercent(h,w5)) assert(topPercent(h,w2));
            if (topPercent(h,w2)) assert(topPercent(h,w1));
        }
    }
    for (int depth=1;depth<=10;++depth) {
        const int normal=shortStackWidth(depth,3,0,0);
        assert(shortStackWidth(depth,3,0,-10)<=normal);
        assert(shortStackWidth(depth,3,0,10)>=normal);
    }
    assert(shortStackWidth(3,3,0)>shortStackWidth(3,10,3));
    assert(shortStackWidth(3,3,0,0,true)
        <shortStackWidth(3,3,0,0,false));
    assert(shortStackWidth(3,3,0,0,true,3)
        <shortStackWidth(3,3,0,0,true,0));
    assert(shortStackWidth(15,2,0)==0);
    assert(comboCountInTopPercent(100)==1326);
    assert(comboCountInTopPercent(0)==0);

    // A sample of neighboring hands around KTo stays in the same broad
    // sub-1BB heads-up survival bucket. This is deliberately NOT a single
    // KTo-specific exception.
    const int emergency=shortStackWidth(0.87,2,0,0,false);
    for (Hand h : {Hand{13,10,false,false},Hand{13,11,false,false},
                   Hand{12,10,false,false},Hand{12,11,false,false},
                   Hand{11,10,false,false},Hand{14,9,false,false},
                   Hand{10,10,false,true},Hand{9,9,false,true}})
        assert(topPercent(h,emergency));
    assert(!topPercent({7,2,false,false},
                       shortStackWidth(5,10,3)));
    std::cout<<"combo-weighted ranges and widening invariants passed\n";
}
