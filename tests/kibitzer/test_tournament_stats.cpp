#include "kibitzer_tournament_stats.h"
#include <cassert>
#include <cmath>
#include <vector>
using namespace KibitzerTournamentStats;
static void near(double a,double b) { assert(std::fabs(a-b)<1e-9); }
int main() {
    // 40 x +10BB, 40 x +2BB, 20 x -1BB = 460 BB per 100 hands.
    // Each blind level must normalize its OWN result, not the final stack.
    std::vector<HandResult> hands;
    for(int i=0;i<40;++i) hands.push_back({1000,1200,20,true});
    for(int i=0;i<40;++i) hands.push_back({1000,1200,100,true});
    for(int i=0;i<20;++i) hands.push_back({2000,1500,500,true});
    auto x = summarize(hands,{});
    assert(x.hands==100);
    near(x.totalBb,460.0);
    near(x.bbPer100,460.0);
    // Folded hands and blind-only walks must still enter the denominator.
    hands.push_back({1000,980,20,true});
    // Older hands without settlement and invalid blind data MUST NOT enter.
    hands.push_back({0,0,0,false});
    hands.push_back({100,200,0,true});
    x=summarize(hands,{});
    assert(x.hands==101);
    near(x.totalBb,459.0);
    near(x.bbPer100,459.0*100.0/101.0);
    // Exactly one finished tournament gives 1st place and one win.
    x=summarize({},{{6,1,true},{6,4,true},{10,0,false}});
    assert(x.completedTournaments==2 && x.wins==1 && x.abandonedTournaments==1);
    near(x.averagePlace,2.5);
    near(x.averageNormalizedPlace,0.3);
    x=summarize({},{{9,3,true},{8,0,true},{1,1,true}});
    assert(x.completedTournaments==1 && x.wins==0);
    near(x.averagePlace,3);
    // Previous hand survivors matter, not original tournament entrants.
    assert(eliminatedPlace(3)==4);
    assert(!sharedElimination(4,3));  // ordinary elimination (possibly after prior bust-outs)
    assert(sharedElimination(4,2));   // two players gone in the SAME hand
    assert(!sharedElimination(2,1));
    assert(!admissible({-1,0,20,true}));
    return 0;
}
