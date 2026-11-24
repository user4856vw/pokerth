#include "kibitzer_preflop_policy.h"
#include <cassert>
#include <iostream>

using namespace KibitzerPreflop;
using KibitzerAutoplay::Action;

static Input baseline(int a,int b,int button=4) {
    Input i;
    i.card0=a; i.card1=b; i.button=button; i.alive=10;
    i.bigBlind=20; i.highestSet=20; i.price=20;
    i.stack=5000; i.minRaise=40; i.maxRaise=5000;
    return i;
}
int main() {
    // Regression from overnight tournament 23, hand 7:
    // A-diamond K-spade, unopened 10-player pot, 30 in pot, 20 to call.
    // The old all-opponents-equity test auto-folded it.
    {
        const auto d=choose(baseline(12,37,4));
        assert(d.action==Action::Raise && d.amount>=40);
    }
    {
        const auto d=choose(baseline(12,37,10));
        assert(d.action==Action::Raise && d.amount>=40);
    }
    // Position sensitivity: small pairs are late-position opens, not UTG opens.
    {
        const auto early=choose(baseline(0,39,4));
        const auto button=choose(baseline(0,39,1));
        assert(early.action==Action::Fold);
        assert(button.action==Action::Raise);
    }
    // A weak, offsuit trash hand is not automatically profitable on the button.
    assert(choose(baseline(5,39,1)).action==Action::Fold); // 7d 2c

    // Unraised walk: do not bet merely because showdown equity is positive.
    {
        auto i=baseline(12,37,3);
        i.price=0;
        assert(choose(i).action==Action::Check);
    }
    // Open raise changes the situation: do not use equity vs 9 random hands.
    {
        auto i=baseline(0,39,1);
        i.hasRaise=true; i.highestSet=100; i.price=100; i.minRaise=180;
        assert(choose(i).action==Action::Fold);
        i.card0=12; i.card1=37;
        auto d=choose(i);
        assert(d.action==Action::Raise && d.amount>=180);
    }
    // Never auto-call an unmodeled multiway stack-off with pocket deuces.
    {
        auto i=baseline(0,39,1);
        i.hasRaise=true; i.highestSet=5000; i.price=5000;
        i.opponentAllIns=3;
        assert(choose(i).action==Action::Fold);
        i.card0=11; i.card1=24; // Kd Kh
        assert(choose(i).action==Action::Call);
    }
    // Position does not override unmodeled multiway limp pressure.
    {
        auto i=baseline(5,39,1);
        i.limpers=4;
        assert(choose(i).action==Action::Fold);
    }
    // A short-stack value hand is an explicitly provisional push.
    {
        auto i=baseline(12,37,4);
        i.stack=160; i.maxRaise=160; i.minRaise=40;
        assert(choose(i).action==Action::Raise &&
               choose(i).amount==160);
    }
    // Strong invariant: all six pocket-aces combinations continue preflop
    // in a standard chip-EV tournament, including versus several all-ins.
    {
        const int aces[]={12,25,38,51};
        for (int x=0;x<4;++x) for (int y=x+1;y<4;++y) {
            for (int scenario=0;scenario<6;++scenario) {
                auto i=baseline(aces[x],aces[y],scenario%2 ? 1 : 4);
                switch (scenario) {
                    case 0: break; // unopened
                    case 1: i.limpers=4; break;
                    case 2: i.hasRaise=true; i.highestSet=200;
                            i.price=200; i.minRaise=380; break;
                    case 3: i.hasRaise=true; i.highestSet=5000;
                            i.price=5000; i.minRaise=0; i.maxRaise=0;
                            i.opponentAllIns=3; break;
                    case 4: i.hasRaise=true; i.highestSet=2500;
                            i.price=2500; i.minRaise=6000; i.maxRaise=5000;
                            break; // raise unavailable; call must remain legal
                    case 5: i.button=3; i.price=0;
                            i.minRaise=0; i.maxRaise=0; break; // walk
                }
                const auto d=choose(i);
                assert(d.action!=Action::Fold);
                if (i.price>0)
                    assert(d.action==Action::Call || d.action==Action::Raise);
                if (d.action==Action::Call) assert(d.amount==i.price);
                if (d.action==Action::Raise)
                    assert(d.amount>=i.minRaise && d.amount<=i.maxRaise);
            }
        }
    }
    // Fail closed on invalid card code and illegal raise bounds.
    {
        auto i=baseline(-1,37,4);
        assert(choose(i).action==Action::Fold);
        i=baseline(12,37,4);
        i.minRaise=10000; i.maxRaise=5000;
        assert(choose(i).action==Action::Fold);
    }
    std::cout<<"preflop policy regressions passed\n";
}
