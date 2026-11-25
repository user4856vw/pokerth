#include "kibitzer_preflop_position.h"
#include "kibitzer_preflop_policy.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace KibitzerPreflopPosition;
int main() {
    // All ten seats exist but only four players remain. Player 0 is UTG,
    // not "unknown=early" because button==0.
    std::vector<Seat> seats;
    for(int i=0;i<10;++i)
        seats.push_back({i,i==2?1:0,i==0||i==2||i==5||i==8});
    auto r=derive(seats);
    assert(r.alive==4 && r.heroRole==3 && r.dealerId==2);

    // Three-handed: player 0 BB, button on player 3, SB at 7.
    for(auto &s:seats) {
        s.alive=(s.id==0||s.id==3||s.id==7);
        s.button=s.id==3?1:0;
    }
    r=derive(seats);
    assert(r.alive==3 && r.heroRole==2 && r.dealerId==3);

    // Heads-up: dealer is also SB; hero opposite the dealer is BB.
    for(auto &s:seats) {
        s.alive=(s.id==0||s.id==7);
        s.button=s.id==7?1:0;
    }
    r=derive(seats);
    assert(r.alive==2 && r.heroRole==1 && r.dealerId==7);
    for(auto &s:seats) s.button=s.id==0?1:0;
    r=derive(seats);
    assert(r.alive==2 && r.heroRole==0 && r.dealerId==0);

    // The marker "0" is not equivalent to UTG. The true role must
    // determine whether an unmarked hero is early or late.
    assert(KibitzerPreflop::position(0,10,3)==KibitzerPreflop::Position::Early);
    assert(KibitzerPreflop::position(0,5,4)==KibitzerPreflop::Position::Late);
    assert(KibitzerPreflop::position(0,3,0)==KibitzerPreflop::Position::Button);
    assert(KibitzerPreflop::position(0,2,1)==KibitzerPreflop::Position::BigBlind);

    // Unknown dealer must not silently invent a position.
    for(auto &s:seats) s.button=0;
    r=derive(seats);
    assert(r.alive==2 && r.heroRole==-1);
    std::cout<<"surviving seat ring positioning passed\n";
}
