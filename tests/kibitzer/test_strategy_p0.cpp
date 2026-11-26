#include "kibitzer_strategy_p0.h"
#include "kibitzer_preflop_position.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace KibitzerBoard;
using namespace KibitzerOpponent;
using KibitzerAutoplay::Action;
using KibitzerAutoplay::Decision;

static KibitzerAutoplay::Input post(int price,int pot,int stack,double eq){
    KibitzerAutoplay::Input in;
    in.adviceReady=true;in.rangesUsable=true;in.price=price;in.pot=pot;in.stack=stack;
    in.opponents=1;in.equity=eq;in.required=double(price)/(pot+price);in.margin=.12;
    return in;
}
static void verifyRange(const Range& r,int a,int b,const Board& board,int n){
    double sum=0;const auto known=mask(a,b,board,n);
    for(const auto& c:r){
        assert(c.a<c.b && !((1ULL<<c.a|1ULL<<c.b)&known));
        assert(c.weight>0&&std::isfinite(c.weight));sum+=c.weight;
    }
    assert(std::abs(sum-1)<1e-10);
}
int main(){
    // Real PokerTH HU blind markers, including nonadjacent seat IDs.
    using KibitzerPreflopPosition::Seat;
    const std::vector<Seat> seats{{0,3,true},{1,0,false},{7,2,true}};
    auto pos=KibitzerPreflopPosition::derive(seats,0);
    assert(pos.alive==2&&pos.heroRole==1&&pos.dealerId==7);
    pos=KibitzerPreflopPosition::derive(seats,7);
    assert(pos.heroRole==0);
    assert(KibitzerPreflopPosition::derive({{0,0,true},{7,3,true}},0).heroRole==-1);

    // Original Qc Jh: Ts Tc 7d Kd Ks, exact public cards from the journal.
    const int a=49,b=22;Board board{{34,47,5,11,37}};
    const auto flop=analyze(a,b,board,3),turn=analyze(a,b,board,4),river=analyze(a,b,board,5);
    assert(flop.valid&&flop.boardPairs==1&&!flop.privateHand&&flop.nominalDrawOuts==0);
    assert(turn.valid&&!turn.privateHand&&turn.nominalDrawOuts==8&&turn.unbeatableDrawOuts==0);
    assert(river.valid&&river.boardPairs==2&&!river.privateHand);
    // Opponent betting history reconstructed as a deliberately stated
    // single-aggressor fixture (historical exports lack individual seat IDs).
    std::vector<Observation> line{
        {1,0,5,540,240,160,0,5000,9},
        {1,1,4,816,1320,0,0,4460,2},
        {1,2,4,1826,2952,0,0,3644,2},
        {1,3,4,1253,6604,0,0,1818,2}
    };
    auto prior=uniform(a,b,board,4);
    auto posteriorRange=posterior(prior,board,4,1,line,160,Profile::Balanced);
    verifyRange(posteriorRange,a,b,board,4);
    auto m=assess(a,b,board,4,{1},line,160,5000,17,turn);
    assert(m.valid&&m.balanced.exact&&m.aggressiveStreets==3);
    assert(m.balanced.samples==1035*44);
    assert(m.balanced.drawProbability>0&&m.balanced.drawWinShare<m.balanced.drawProbability);
    auto in=post(1826,4778,3253,.528);
    auto old=KibitzerAutoplay::choose(in);assert(old.action==Action::Call);
    const auto revised=KibitzerStrategy::postflop(in,old,turn,m,4,4778,false,false);
    std::cout<<"original QJ turn "<<m.valueHeavy.equity<<" "<<m.balanced.equity<<" "<<m.bluffHeavy.equity<<" required "<<in.required<<" "<<revised.reason<<std::endl;
    assert(revised.action==Action::Fold);

    // Exact random HU river cross-check (990 compatible combos).
    const auto r=exactHU(a,b,board,5,uniform(a,b,board,5),river);
    assert(r.valid&&r.exact&&r.samples==990);
    assert(std::abs(r.equity-(river.currentAhead+.5*river.currentTie))<1e-10);
    // Suit permutation must preserve all exact diagnostics.
    auto perm=[](int x){return ((x/13+1)%4)*13+x%13;};
    Board permBoard=board;for(auto& x:permBoard)x=perm(x);
    auto permTurn=analyze(perm(a),perm(b),permBoard,4);
    assert(permTurn.nominalDrawOuts==turn.nominalDrawOuts&&permTurn.unbeatableDrawOuts==turn.unbeatableDrawOuts);
    assert(std::abs(permTurn.currentAhead-turn.currentAhead)<1e-10);
    // Legitimate strong made hand: kings full on the same paired turn.
    auto strong=analyze(24,37,board,4);
    assert(strong.privateHand&&strong.madeCategory==6);
    auto strongM=assess(24,37,board,4,{1},line,160,5000,17,strong);
    auto strongIn=post(1826,4778,3253,.99);
    assert(KibitzerStrategy::postflop(strongIn,KibitzerAutoplay::choose(strongIn),
        strong,strongM,4,4778,false,false).action==Action::Call);
    // Even with unavailable action history a costly paired-board
    // air/draw call must not rely on unconditioned equity alone.
    assert(KibitzerStrategy::postflop(in,old,turn,{},4,4778,false,false).action==Action::Fold);

    // Actual overnight KJo 15.175 BB call, and neighboring marginal hands.
    KibitzerPreflop::Input p;
    p.card0=48;p.card1=24;p.alive=10;p.roleIndex=9;p.stack=4970;
    p.bigBlind=40;p.highestSet=607;p.price=607;p.pot=802;p.hasRaise=true;
    for(auto hole:{std::array<int,2>{48,24},{49,22},{47,22},{49,21}}){
        p.card0=hole[0];p.card1=hole[1];
        auto d=KibitzerStrategy::preflop(p,{Action::Call,p.price,true,"fixture-old-call"},{},5,.23,802,false);
        assert(d.action==Action::Fold);
    }
    // Actual original QJo multiway expensive raise; no model required
    // for the very large random-equity deficit of this marginal hand.
    p.card0=a;p.card1=b;p.alive=9;p.roleIndex=7;p.stack=4609;p.bigBlind=160;
    p.price=p.highestSet=540;p.pot=780;
    assert(KibitzerStrategy::preflop(p,{Action::Call,540,true,"fixture-old-call"},{},5,.231,780,false).action==Action::Fold);
    // Affordable ordinary QJo defense with good pot odds is not banned.
    p.price=p.highestSet=320;p.pot=2000;
    assert(KibitzerStrategy::preflop(p,{Action::Call,320,true,"fixture-old-call"},{},1,.48,2000,false).action==Action::Call);
    // AA invariant and the true original sub-1BB KTo call.
    p.card0=12;p.card1=25;p.price=p.highestSet=3000;p.pot=4000;
    assert(KibitzerStrategy::preflop(p,{Action::Call,3000,true,"aa"},{},5,.4,4000,false).action==Action::Call);
    p.card0=50;p.card1=21;p.alive=2;p.roleIndex=0;p.bigBlind=5120;
    p.stack=1880;p.heroCommitted=2560;p.price=1880;p.highestSet=5120;p.pot=7680;
    p.hasRaise=false;p.opponentAllIns=0;p.minRaise=p.maxRaise=0;
    auto shortOld=KibitzerPreflop::choose(p);
    assert(shortOld.action==Action::Call);
    assert(KibitzerStrategy::preflop(p,shortOld,{},1,.595,7680,false).action==Action::Call);
    // Actual overnight K6o cheap BB call against a short all-in.
    p.card0=37;p.card1=4;p.alive=2;p.roleIndex=1;p.bigBlind=5120;
    p.stack=39170;p.heroCommitted=5120;p.price=590;p.highestSet=5710;
    p.pot=10830;p.hasRaise=true;p.opponentAllIns=1;
    const Board empty{{-1,-1,-1,-1,-1}};
    auto hu=assess(p.card0,p.card1,empty,0,{7},
                   {{7,0,6,3150,7680,5120,2560,3150,2}},5120,5000,117,{});
    assert(hu.valid);
    assert(KibitzerStrategy::preflop(p,{Action::Fold,0,true,"legacy-fold"},hu,1,.553,10830,false).action==Action::Call);
    assert(KibitzerStrategy::preflop(p,{Action::Fold,0,true,"legacy-fold"},hu,1,.553,10830,true).action==Action::Fold);
    // Multiway joint rejection, reproducibility, and no incompatible card draws.
    auto pri=uniform(12,25,empty,0);normalize(pri);
    const auto mc1=monteCarlo(12,25,empty,0,{pri,pri},1200,123);
    const auto mc2=monteCarlo(12,25,empty,0,{pri,pri},1200,123);
    assert(mc1.valid&&mc1.equity==mc2.equity&&mc1.samples==mc2.samples);
    std::cout<<"P0 board, weighted ranges, preflop and HU regressions passed\n";
}
