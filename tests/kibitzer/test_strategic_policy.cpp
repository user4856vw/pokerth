#include "kibitzer_strategic_policy.h"
#include "kibitzer_preflop_policy.h"
#include "kibitzer_preflop_position.h"
#include <cassert>
#include <iostream>
using namespace KibitzerBoard;
using namespace KibitzerAutoplay;
static State state(int a,int b,std::initializer_list<int> board){
    State s;s.h0=a;s.h1=b;s.n=int(board.size());
    std::copy(board.begin(),board.end(),s.board.begin());return s;
}
static Decision replay(const State& s,const std::vector<Evidence>& history,
                       int price,int pot,int stack,double random){
    Input in;in.adviceReady=true;in.price=price;in.pot=pot;in.stack=stack;
    in.opponents=1;in.equity=random;in.required=double(price)/(price+pot);
    auto t=analyze(s);auto e=estimate(s,history,10000,19);
    assert(t.ready&&e.ready);
    auto d=KibitzerStrategic::choose(in,t,e,int(history.size()),true,choose(in));
    std::cout<<"price="<<price<<" odds="<<in.required<<" equity="
             <<e.equity[0]<<","<<e.equity[1]<<","<<e.equity[2]
             <<" nominal="<<t.straightOrFlushOuts<<" effective="
             <<e.winningDrawOuts[0]<<" reason="<<d.reason<<"\n";
    return d;
}
int main(){
    // Known categories, kickers, wheel, double trips, two pair and invalid cards.
    assert(category(value(Cards{{0,1,2,3,4,18,32}},7))==8);
    assert(value(Cards{{0,1,2,3,12,18,32}},7)==encode(8,5));
    assert(value(Cards{{12,25,38,51,11,1,2}},7)==encode(7,14,13));
    assert(value(Cards{{12,25,38,11,24,37,2}},7)==encode(6,14,13));
    assert(value(Cards{{12,25,11,24,10,23,0}},7)==encode(2,14,13,12));
    assert(value(Cards{{0,0,1,2,3,4,5}},7)==-1);
    // Exact historical QJ turn, log 2026-11-25 10:26:04Z.
    const auto flop=state(49,22,{34,47,5}),turn=state(49,22,{34,47,5,11});
    assert(analyze(flop).paired&&!analyze(flop).privateMade);
    assert(analyze(turn).straightOrFlushOuts==8);
    auto estimateTurn=estimate(turn,{{flop,816.0/1320},{turn,1826.0/2952}},5000,19);
    assert(estimateTurn.samples==45540&&estimateTurn.ready);
    assert(estimateTurn.winningDrawOuts[0]<8);
    assert(replay(turn,{{flop,816.0/1320},{turn,1826.0/2952}},1826,4778,3253,.5275).action==Action::Fold);
    // Two overnight paired-board failures: prior street observations are
    // explicitly assumed to be the same bettor for this scenario fixture.
    const auto a9f=state(38,20,{36,41,49}),a9t=state(38,20,{36,41,49,19}),a9r=state(38,20,{36,41,49,19,43});
    assert(replay(a9r,{{a9f,272.0/440},{a9t,608.0/984},{a9r,1361.0/2200}},1361,3561,2133,.4333).action==Action::Fold);
    const auto qjf=state(22,36,{4,25,18}),qjt=state(22,36,{4,25,18,38}),qjr=state(22,36,{4,25,18,38,51});
    assert(replay(qjr,{{qjf,519.0/1080},{qjt,1019.0/2118},{qjr,2000.0/4156}},2000,6156,2616,.5048).action==Action::Fold);
    // Opposite case: nut full house / quads on dangerous board, not blanket fold.
    const auto nuts=state(12,25,{38,51,4,18,36});
    assert(analyze(nuts).privateMade);
    auto e=estimate(nuts,{{nuts,.65}},5000,19);
    assert(e.ready&&e.samples==990&&e.equity[0]==1);
    assert(replay(nuts,{{nuts,.65}},2000,6000,2500,1).action==Action::Call);
    // Exact unweighted river agrees with exhaustive equity; suit permutation.
    const auto river=state(49,22,{34,47,5,11,37});
    auto base=estimate(river,{},5000,19);
    assert(base.ready&&base.samples==990);
    assert(std::abs(base.equity[0]-.6121212121212121)<.000001);
    auto perm=river;perm.h0=(perm.h0+13)%52;perm.h1=(perm.h1+13)%52;
    for(auto& x:perm.board)x=(x+13)%52;
    auto same=estimate(perm,{},5000,19);
    assert(std::abs(base.equity[0]-same.equity[0])<1e-12);
    // Heads-up seat markers and actual cheap K6 against covered short all-in.
    using KibitzerPreflopPosition::Seat;
    const std::vector<Seat> seats{{0,3,true},{4,2,true},{8,0,false}};
    auto pos=KibitzerPreflopPosition::derive(seats);
    assert(pos.heroRole==1&&pos.dealerId==4);
    KibitzerPreflop::Input p;p.card0=4;p.card1=37;p.alive=2;p.roleIndex=1;
    p.button=3;p.bigBlind=5120;p.highestSet=5710;p.heroCommitted=5120;
    p.price=590;p.pot=10830;p.stack=39170;p.hasRaise=true;p.opponentAllIns=1;
    p.strategicV6=true;p.opponents=1;p.randomEquity=.553;
    const auto pre=state(4,37,{});p.conditionedLow=estimate(pre,{{pre,.1}},10000,19).equity[0];
    assert(KibitzerPreflop::choose(p).action==Action::Call);
    // Original QJ preflop: 5 potential opponents, negative random equity.
    p={};p.card0=49;p.card1=22;p.alive=9;p.roleIndex=0;p.bigBlind=160;
    p.highestSet=540;p.price=540;p.pot=780;p.stack=4609;p.hasRaise=true;
    p.strategicV6=true;p.opponents=5;p.randomEquity=.2309;
    assert(KibitzerPreflop::choose(p).action==Action::Fold);
    p.price=240;p.highestSet=240;p.pot=800;p.opponents=1;p.randomEquity=.5;
    p.conditionedLow=.35;assert(KibitzerPreflop::choose(p).action==Action::Call);
    p.card0=12;p.card1=25;p.highestSet=4000;p.price=4000;p.opponentAllIns=1;
    assert(KibitzerPreflop::choose(p).action==Action::Call);
    std::cout<<"v6 strategic regression suite passed\n";
}
