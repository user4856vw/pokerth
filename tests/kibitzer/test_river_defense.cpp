#include "kibitzer_river_defense.h"
#include <cassert>
#include <iostream>
using namespace KibitzerRiverDefense;
int main() {
    // Overnight board Jc-Qh-4d-4h-5s, hero As-Ad.
    // Even AA loses to a full house, but can bluff-catch a small raise.
    Input a;
    a.hero0=38; a.hero1=12;
    a.board={{48,23,2,15,29}};
    a.pot=1200; a.remainingStack=1000; a.price=20;
    assert(hasShowdownHand(a.hero0,a.hero1,a.board));
    assert(allowCheapCall(a));
    a.price=60; assert(allowCheapCall(a)); // exactly 6% of remaining
    a.price=61; assert(!allowCheapCall(a)); // stack risk limit
    a.price=1557; a.pot=5451; a.remainingStack=2111;
    assert(!allowCheapCall(a)); // the original disastrous river call
    a.price=0; assert(!allowCheapCall(a));
    a.price=20; a.pot=100; assert(!allowCheapCall(a)); // >12% pot
    a.pot=1200; a.remainingStack=100;
    assert(!allowCheapCall(a)); // same 20 chips, too much of stack

    // A paired board alone does not make irrelevant hole cards worth a call.
    a.hero0=0; a.hero1=5; // 2d/7d, no pair with J-Q-4-4-5
    a.remainingStack=1000;
    assert(!hasShowdownHand(a.hero0,a.hero1,a.board));
    assert(!allowCheapCall(a));
    a.hero0=2; // duplicate 4d board card: invalid game state
    assert(!allowCheapCall(a));

    // No accidental low-card-high-card pair inference, no integer overflow.
    a.hero0=38; a.hero1=12;
    a.board={{48,23,2,15,29}};
    a.price=1'000'000'000; a.pot=2'000'000'000;
    a.remainingStack=1'500'000'000;
    assert(!allowCheapCall(a));
    std::cout << "river bluff-catch boundaries passed\n";
}
