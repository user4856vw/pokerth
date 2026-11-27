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

    // v4 overnight repeats: exact public board, price and remaining
    // stack from session 16. Same-aggressor sequences are test assumptions,
    // not recovered private cards or historical opponent IDs.
    {
        Board qj{{4,25,18,38,51}}; // 6d Ah 7h As Ac; Jh Qs
        std::vector<Observation> h{{1,0,5,420,240,160,0,5000,9},
            {1,1,4,519,1080,0,0,4580,2},
            {1,2,4,1019,2118,0,0,4061,2},
            {1,3,4,2000,4156,0,0,3042,2}};
        auto t=analyze(22,36,qj,4);
        assert(t.valid&&!t.privateHand&&t.boardPairs==1&&t.nominalDrawOuts==0);
        auto mt=assess(22,36,qj,4,{1},h,160,5000,17,t);
        auto it=post(1019,3137,3635,.469);
        auto dt=KibitzerStrategy::postflop(it,KibitzerAutoplay::choose(it),t,mt,4,3137,false,false);
        std::cout<<"overnight QJ turn "<<mt.valueHeavy.equity<<" "<<mt.balanced.equity<<" "<<mt.bluffHeavy.equity<<" "<<dt.reason<<std::endl;
        assert(dt.action==Action::Fold);
        auto r2=analyze(22,36,qj,5);
        assert(!r2.privateHand&&r2.maxBoardRank==3);
        auto mr=assess(22,36,qj,5,{1},h,160,5000,17,r2);
        auto ir=post(2000,6156,2616,.505);
        auto dr=KibitzerStrategy::postflop(ir,KibitzerAutoplay::choose(ir),r2,mr,5,6156,false,false);
        std::cout<<"overnight QJ river "<<mr.valueHeavy.equity<<" "<<mr.balanced.equity<<" "<<mr.bluffHeavy.equity<<" "<<dr.reason<<std::endl;
        assert(dr.action==Action::Fold);
    }
    {
        Board a9{{36,41,49,19,43}}; // Qs 4c Qc 8h 6c; As 9h
        std::vector<Observation> h{{1,0,5,200,120,80,0,5000,10},
            {1,1,4,272,440,0,0,4800,2},
            {1,2,4,608,984,0,0,4528,2},
            {1,3,4,1361,2200,0,0,3920,2}};
        auto t=analyze(38,20,a9,4);
        assert(!t.privateHand&&t.nominalDrawOuts==0);
        auto mt=assess(38,20,a9,4,{1},h,80,5000,17,t);
        auto it=post(608,1592,2741,.60);
        auto dt=KibitzerStrategy::postflop(it,KibitzerAutoplay::choose(it),t,mt,4,1592,false,false);
        std::cout<<"overnight A9 turn "<<mt.valueHeavy.equity<<" "<<mt.balanced.equity<<" "<<mt.bluffHeavy.equity<<" "<<dt.reason<<std::endl;
        assert(dt.action==Action::Fold);
        auto r2=analyze(38,20,a9,5);
        assert(!r2.privateHand&&r2.maxBoardSuit==3);
        auto mr=assess(38,20,a9,5,{1},h,80,5000,17,r2);
        auto ir=post(1361,3561,2133,.60);
        auto dr=KibitzerStrategy::postflop(ir,KibitzerAutoplay::choose(ir),r2,mr,5,3561,false,false);
        std::cout<<"overnight A9 river "<<mr.valueHeavy.equity<<" "<<mr.balanced.equity<<" "<<mr.bluffHeavy.equity<<" "<<dr.reason<<std::endl;
        assert(dr.action==Action::Fold);
    }

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
    // V7 incident fixtures. Public cards, pot, price and stack are recorded;
    // range estimates are logged summaries (rounded to five decimals),
    // not newly simulated opponent cards.
    auto logged=[](double balanced,double value,double bluff,double se){
        Assessment x;x.valid=true;x.observedAggression=true;
        x.balanced.valid=x.valueHeavy.valid=x.bluffHeavy.valid=true;
        x.balanced.equity=balanced;x.valueHeavy.equity=value;x.bluffHeavy.equity=bluff;
        x.balanced.standardError=x.valueHeavy.standardError=x.bluffHeavy.standardError=se;
        return x;
    };
    {
        KibitzerPreflop::Input k;
        k.card0=11;k.card1=50;k.alive=10;k.roleIndex=7;k.bigBlind=80;
        k.highestSet=2160;k.heroCommitted=720;k.stack=4280;k.price=1440;
        k.pot=3640;k.hasRaise=true;k.heroPreviouslyRaised=true;
        auto m=logged(.63510,.63240,.65193,.00679);
        auto legacy=KibitzerPreflop::choose(k);
        assert(legacy.action==Action::Fold);
        for(int cost:{1426,1427,1440}){
            k.price=cost;k.highestSet=k.heroCommitted+cost;
            auto d=KibitzerStrategy::preflopV7(k,KibitzerPreflop::choose(k),m,2,.683,
                k.pot,false,3,{});
            assert(d.action==Action::Call&&d.amount==cost);
        }
        // Price/equity, not hand label or already invested chips, determines
        // whether a call is supported. A sufficiently adverse range folds.
        k.price=1440;
        assert(KibitzerStrategy::preflopV7(k,legacy,logged(.15,.16,.17,.001),
            2,.683,k.pot,false,3,{}).action==Action::Fold);
        assert(KibitzerStrategy::preflopV7(k,legacy,{},2,.683,k.pot,false,3,{}).action==Action::Fold);
    }
    {
        // QQ four-bet incident: first decision before hero's 2205-chip raise.
        KibitzerPreflop::Input q;
        q.card0=49;q.card1=10;q.alive=10;q.roleIndex=0;q.bigBlind=80;
        q.highestSet=q.price=735;q.stack=5210;q.pot=1065;q.hasRaise=true;
        q.minRaise=1260;q.maxRaise=5210;
        auto oldQ=KibitzerPreflop::choose(q);
        assert(oldQ.action==Action::Raise&&oldQ.amount==2205);
        auto qm=logged(.40274,.39828,.41859,.00695);
        assert(KibitzerStrategy::preflopV7(q,oldQ,qm,4,.43,q.pot,false,2,{}).action!=Action::Raise);
        // Second recorded state is a counterfactual once the first changes.
        q.highestSet=3494;q.heroCommitted=2205;q.price=1289;q.stack=3005;
        q.pot=9313;q.opponentAllIns=1;q.heroPreviouslyRaised=true;
        const auto bound=KibitzerStrategy::preflopPotBound(q.pot,q.heroCommitted,q.price,
            {3494,3494,80,40});
        assert(bound.valid&&bound.eligible==9313);
        auto oldFold=KibitzerPreflop::choose(q);assert(oldFold.action==Action::Fold);
        auto m=logged(.54563,.53777,.57150,.00702);
        assert(KibitzerStrategy::preflopV7(q,oldFold,m,2,.60,q.pot,true,4,bound).action==Action::Call);
        assert(KibitzerStrategy::preflopV7(q,oldFold,m,2,.60,q.pot,true,4,{}).action==Action::Fold);
        // Opponent overbet excluded, dead money included, invalid ledger rejected.
        auto capped=KibitzerStrategy::preflopPotBound(1200,100,200,{900,200});
        assert(capped.valid&&capped.eligible==600);
        assert(!KibitzerStrategy::preflopPotBound(1201,100,200,{900,200}).valid);
        assert(!KibitzerStrategy::preflopPotBound(500,100,300,{200,200}).valid);
        assert(!KibitzerStrategy::preflopPotBound(1200,100,200,{1101,-1}).valid);
        // Preflop raises and price-increasing all-ins count; all-in calls,
        // ordinary calls and postflop bets do not.
        const std::vector<Observation> trace{
            {1,0,5,210,120,80,0,5000,10},
            {2,0,5,735,330,210,0,5000,9},
            {0,0,3,735,1065,735,0,5210,8},
            {1,0,6,525,1800,735,210,525,8},
            {2,0,6,1,2325,735,735,1,8},
            {2,1,4,100,2326,0,0,100,2}};
        assert(KibitzerStrategy::preflopPriceIncreases(trace)==3);
    }
    {
        // All 26 overnight raise-then-fold records. Conditional replay of
        // the decision snapshots, not a claim to replay entire tournaments.
        // Side-pot records lack a verified full ledger here: never promote.
        struct Incident {int price,pot,stack;double b,v,f;bool side;};
        const Incident rows[]={
          {2635,10315,13235,.5259,.5140,.5626,false},
          {350,1000,4840,.38953,.37603,.40560,false},
          {400,840,4520,.61010,.60220,.62240,false},
          {11316,23771,11607,.29970,.29360,.29350,true},
          {100,210,4950,.55690,.54520,.59040,false},
          {2387,10480,2387,.37833,.35910,.42747,true},
          {4057,5523,5463,.42717,.41443,.47363,true},
          {2324,5844,9003,.53810,.51980,.57690,false},
          {400,840,4450,.49870,.47450,.54000,false},
          {2157,6714,3650,.36533,.35023,.39860,true},
          {237,437,4920,.61640,.61630,.65020,false},
          {2627,7347,3540,.41667,.38237,.44253,true},
          {600,1220,4841,.43117,.42810,.45363,true},
          {400,840,4800,.48730,.45440,.52650,false},
          {2375,4935,3860,.40153,.38780,.42067,true},
          {1600,3360,4542,.61990,.60240,.63230,false},
          {1178,5338,8640,.53440,.51850,.57330,false},
          {700,1220,4620,.58450,.54650,.59840,false},
          {2620,5740,2620,.54510,.52610,.58090,false},
          {899,1859,2220,.55640,.51980,.59490,false},
          {200,420,4890,.50410,.48320,.54610,false},
          {1440,3640,4280,.63510,.63240,.65193,false},
          {1200,2240,2494,.58930,.55440,.63150,false},
          {540,980,4820,.67170,.65240,.70110,false},
          {3094,6934,6846,.60980,.58830,.62380,false},
          {1289,9313,3005,.54563,.53777,.57150,true}
        };
        int promoted=0,sideHeld=0;
        for(const auto& z:rows){
            KibitzerPreflop::Input t;
            // Non-AA placeholder: this regression tests the arithmetic
            // branch independently of hand class; exact KK/QQ above.
            t.card0=11;t.card1=50;t.alive=10;t.bigBlind=80;t.price=z.price;
            t.pot=z.pot;t.stack=z.stack;t.highestSet=z.price;
            t.hasRaise=true;t.heroPreviouslyRaised=true;
            const Decision old{Action::Fold,0,true,"recorded-v6-fold"};
            auto m=logged(z.b,z.v,z.f,.0071); // conservative rounded SE bound
            const auto d=KibitzerStrategy::preflopV7(t,old,m,2,.6,z.pot,z.side,3,{});
            if(z.side){assert(d.action==Action::Fold);++sideHeld;}
            else if(KibitzerStrategy::lower(m)>KibitzerStrategy::odds(z.price,z.pot)){
                assert(d.action==Action::Call&&d.amount==z.price);++promoted;
            }else assert(d.action==Action::Fold);
        }
        std::cout<<"V7 snapshot replay: "<<promoted<<" supported calls, "<<sideHeld
            <<" uncertain side-pot folds held (QQ separately reconciled)\n";
        assert(promoted>0&&sideHeld==8);
    }
    {
        // Exact A8 river incident: trips, paired three-club board.
        Board b{{39,51,10,38,50}};
        const auto a8=analyze(25,6,b,5);
        assert(a8.valid&&a8.boardPairs==1&&a8.maxBoardSuit==3);
        auto i=post(0,3680,3340,.9075);
        const Decision probe{Action::Raise,1840,true,"postflop-value-probe-half-pot-unmodeled-response"};
        assert(KibitzerStrategy::postflopV7(i,probe,a8,{},5,3680,false,false).action==Action::Check);
        auto at=analyze(25,6,b,4);
        assert(KibitzerStrategy::postflopV7(i,probe,at,{},4,3680,false,false).action==Action::Check);
        const Decision modeled{Action::Raise,1840,false,"explicit-conditional-bet-ev-raise"};
        assert(KibitzerStrategy::postflopV7(i,modeled,a8,{},5,3680,false,true).action==Action::Raise);
        // Shove algebra: zero-equity risk, positive called EV, invalid input.
        const double f=KibitzerStrategy::shoveBreakEvenFold(960,5300,5300,0);
        assert(std::abs(f-5300.0/6260.0)<1e-12);
        const double g=KibitzerStrategy::shoveBreakEvenFold(960,5300,5300,.30);
        assert(std::abs(g-(5300-.30*(960+10600))/(960+5300-.30*(960+10600)))<1e-12);
        assert(KibitzerStrategy::shoveBreakEvenFold(960,5300,5300,1)==0);
        assert(std::isnan(KibitzerStrategy::shoveBreakEvenFold(0,5300,5300,.3)));
    }
    std::cout<<"P0 and V7 incident regressions passed\n";
}
