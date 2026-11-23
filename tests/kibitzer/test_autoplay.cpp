#include "kibitzer_autoplay_logic.h"
#include <cassert>
#include <iostream>
using namespace KibitzerAutoplay;

int main()
{
    Input a;
    // The policy never folds when a check is free, even with no model.
    assert(choose(a).action == Action::Check);
    assert(choose(a).provisional);

    a.price = 100;
    assert(choose(a).action == Action::Fold);
    assert(choose(a).provisional);

    a.adviceReady = true;
    a.equity = 0.65; a.required = 0.30; a.margin = 0.12;
    assert(choose(a).action == Action::Call);
    assert(choose(a).provisional); // clear pot odds, but no full Council consensus
    a.council = CouncilStance::Call;
    assert(!choose(a).provisional);
    a.equity = 0.10;
    assert(choose(a).action == Action::Fold);
    assert(choose(a).provisional); // Council previously recommended CALL
    a.council = CouncilStance::Fold;
    assert(!choose(a).provisional);

    a.council = CouncilStance::Abstain;
    a.equity = 0.35; a.required = 0.30;
    assert(choose(a).action == Action::Call && choose(a).provisional);
    a.equity = 0.25;
    assert(choose(a).action == Action::Fold && choose(a).provisional);

    a.price = 0; a.equity = 0.99;
    assert(choose(a).action == Action::Check);
    a.aggressionCandidate = true;
    assert(choose(a).provisional);

    // A suggestion to STUDY a raise is not permission to invent its size.
    a.price = 30; a.equity = 0.80; a.required = 0.20;
    a.minRaise = 60; a.maxRaise = 200;
    assert(choose(a).action == Action::Raise);
    assert(choose(a).amount == 60 && choose(a).provisional);
    a.council = CouncilStance::Passive; // explicit passive evidence blocks an unpriced raise
    assert(choose(a).action == Action::Call);
    a.council = CouncilStance::Abstain;
    a.modeledRaiseEdge = true;
    a.modeledRaiseSize = 80;
    assert(choose(a).action == Action::Raise); // still only Stage A's provisional min raise
    assert(choose(a).amount == 60 && choose(a).provisional);
    a.council = CouncilStance::Raise;
    assert(choose(a).action == Action::Raise);
    assert(choose(a).amount == 80);
    assert(!choose(a).provisional);

    a.modeledRaiseSize = 40; // not a legal modeled raise
    assert(choose(a).action == Action::Call);
    a.modeledRaiseSize = 250;
    assert(choose(a).action == Action::Call);
    a.modeledRaiseSize = 80; a.rangesUsable = false;
    assert(choose(a).action == Action::Fold && choose(a).provisional);
    a.rangesUsable = true;
    a.equity = NAN;
    assert(choose(a).action == Action::Fold && choose(a).provisional);
    std::cout << "All KibitzerAutoplay tests passed\n";
}
