#include "kibitzer_preflop_policy.h"
#include <cassert>
#include <array>
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
    // Overnight v2 incident at 23:47 UTC: Kc-Th, less than 1BB,
    // blinds 2560/5120, 1880 to call into a pot of 7680.
    // Verify a whole neighborhood, rather than special-case KTo.
    {
        auto i=baseline(50,21,0);
        i.roleIndex=0; i.alive=2; i.bigBlind=5120;
        i.stack=4440; i.heroCommitted=0;
        i.price=1880; i.pot=7680; i.highestSet=5120;
        i.minRaise=0; i.maxRaise=0;
        for (const auto &hole : {
                std::array<int,2>{50,21}, // Kc Th
                std::array<int,2>{50,22}, // Kc Jh
                std::array<int,2>{49,21}, // Qc Th
                std::array<int,2>{49,22}, // Qc Jh
                std::array<int,2>{48,21}, // Jc Th
                std::array<int,2>{12,20}, // Ad 9h
                std::array<int,2>{8,21}   // Td Th
            }) {
            i.card0=hole[0]; i.card1=hole[1];
            const auto d=choose(i);
            assert(d.action==Action::Call && d.amount==1880);
        }
    }
    // Other side of the late push/fold boundary: 72o with 5BB in a
    // ten-handed early seat should still be outside the ranked shove chart.
    {
        auto i=baseline(5,39,0);
        i.roleIndex=3; i.alive=10;
        i.stack=100; i.heroCommitted=0; i.bigBlind=20;
        i.price=20; i.pot=30;
        assert(choose(i).action==Action::Fold);
    }
    // Overnight v2 incident at 23:49 UTC: Ah As originally raised,
    // then folded to 2259 into a 3461 pot. That fold is prohibited.
    {
        auto i=baseline(25,38,0);
        i.alive=10; i.roleIndex=4;
        i.stack=4799; i.heroCommitted=201;
        i.hasRaise=true; i.price=2259; i.pot=3461;
        i.highestSet=2460; i.minRaise=0; i.maxRaise=0;
        const auto d=choose(i);
        assert(d.action==Action::Call && d.amount==2259);
    }
    // Configurable value 3-bet chart: threshold classes immediately on
    // each side of the 5/6/7% boundary. No unpriced semi-bluff expansion.
    {
        auto i=baseline(50,48,0); // Kc Jc suited
        i.alive=10; i.roleIndex=0;
        i.price=100; i.highestSet=100; i.hasRaise=true;
        i.minRaise=180;
        i.valueThreeBetPercent=5;
        assert(choose(i).action!=Action::Raise);
        i.valueThreeBetPercent=6;
        assert(choose(i).action==Action::Raise);
        i.card0=6; i.card1=19; // 8d 8h
        assert(choose(i).action!=Action::Raise);
        i.valueThreeBetPercent=7;
        assert(choose(i).action==Action::Raise);
        i.card0=50; i.card1=8; // Kc Td (not in value 3-bet chart)
        assert(choose(i).action!=Action::Raise);
    }
    // Short-stack setting widens playable combinations and never shrinks
    // them when players bust or blind pressure increases.
    {
        auto i=baseline(50,21,0);
        i.alive=3; i.roleIndex=0; i.bigBlind=100;
        i.stack=500; i.heroCommitted=0; i.price=100;
        i.highestSet=100; i.pot=150;
        i.shortStackAdjustment=-10;
        const auto narrow=choose(i);
        i.shortStackAdjustment=10;
        const auto wide=choose(i);
        if (narrow.action!=Action::Fold)
            assert(wide.action!=Action::Fold);
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
