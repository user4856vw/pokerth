#include "kibitzer_empirical_model.h"
#include <cassert>
#include <iostream>
using namespace KibitzerEmpirical;
using KibitzerBoard::Board;

static const Board empty{{-1,-1,-1,-1,-1}};
static const Decision call{Action::Call,1,true,"recorded-v7-call"};
static Assessment replay(const char* label,int a,int b,const Board& board,int n,int bb,
                         const std::vector<int>& seats,const std::vector<Observation>& history,
                         const Plan& plan,bool shouldFold){
    const auto x=assess(a,b,board,n,seats,history,bb,plan,5000,13579,
                       KibitzerBoard::analyze(a,b,board,n));
    assert(x.valid);
    const auto d=applyCall({Action::Call,plan.price,true,"recorded-v7-call"},x,n==0,a,b);
    std::cout<<label<<" power EV "<<x.power.ev<<" tail EV "<<x.tail.ev
             <<" regrets call/fold "<<x.regretCall<<"/"<<x.regretFold
             <<" -> "<<d.reason<<std::endl;
    assert(d.action==(shouldFold?Action::Fold:Action::Call));
    return x;
}
int main(){
    assert(training[0].raises==703&&training[1].raiseOpportunities==1889);
    assert(!rate(0,0).valid&&rate(7,27).hi>rate(7,27).point);
    assert(context({1,0,5,430,160,100,20,4910,3},40,false)==1);
    assert(context({1,0,5,960,520,360,120,4803,2},40,true)==2);
    assert(preflopRank(12,25)==8513&&preflopRank(12,11)>preflopRank(12,24));
    assert(preflopRank(35,37)==preflopRank(9,11));
    assert(preflopRank(11,50)>preflopRank(9,22));
    // Independent engine evaluator check of the rank table. Hoeffding
    // union bound: four classes, two estimates/class, alpha=1e-6.
    const int n=100000;
    const double tol=2*std::sqrt(std::log(16.0/1e-6)/(2*n))+.00005;
    for(auto h:{std::array<int,2>{12,25},{0,18},{35,37},{9,22}}){
        auto r=uniform(h[0],h[1],empty,0);normalize(r);
        const auto e=monteCarlo(h[0],h[1],empty,0,{r},n,20261128);
        assert(e.valid&&std::abs(e.equity-preflopRank(h[0],h[1])/10000.0)<tol);
    }
    // Probability mass and equal-strength symmetry; top-tail selection
    // must not privilege the order of combinations in a tied class.
    Range r{{0,1,.25},{2,3,.25},{4,5,.25},{6,7,.25}};
    auto t=r;
    assert(select(t,{1,2,2,3},.5,Shape::TopTail));
    assert(t[0].weight==0&&std::abs(t[1].weight-.25)<1e-12&&
           t[1].weight==t[2].weight&&std::abs(t[3].weight-.5)<1e-12);
    auto p=r;assert(select(p,{1,2,2,3},.5,Shape::Power));
    assert(std::abs(p[0].weight-.0625)<1e-12&&p[1].weight==p[2].weight&&
           std::abs(p[3].weight-.4375)<1e-12);
    assert(!select(p,{1,2,2,3},0,Shape::Power));
    // Main + side pot, folded dead money, hero ineligible for upper tier.
    auto plan=tierPlan(850,100,0,{{0,200},{1,100},{2,400},{3,150}},{1,2});
    assert(plan.valid&&plan.tiers.size()==3);
    assert(plan.tiers[0].amount==400&&plan.tiers[0].mask==3);
    assert(plan.tiers[1].amount==150&&plan.tiers[1].mask==2);
    assert(plan.tiers[2].amount==300&&plan.tiers[2].mask==2);
    assert(payoff(10,{11,9},plan)==350); // loses main, wins side
    assert(payoff(10,{10,10},plan)==400.0/3+450.0/2-100);
    assert(payoff(12,{11,10},plan)==750);
    assert(!tierPlan(851,100,0,{{0,200},{1,100},{2,400},{3,150}},{1,2}).valid);
    assert(!tierPlan(850,100,0,{{0,200},{1,100},{2,400},{3,150}},{1,1}).valid);
    assert(!tierPlan(850,300,0,{{0,200},{1,100},{2,400},{3,150}},{1,2}).valid);
    // Exact incident: JJ four-bet and subsequent repeated aggression.
    const std::vector<Observation> jj{{4,0,5,120,60,40,0,4923,8},
        {4,0,5,960,520,360,120,4803,2},{4,1,4,1210,2200,0,0,3843,2},
        {4,2,4,2541,4620,0,0,2633,2}};
    replay("JJ preflop",9,22,empty,0,40,{4},{jj.begin(),jj.begin()+2},singlePot(1480,720,1),false);
    auto turn=replay("JJ turn",9,22,Board{{0,49,31,4,-1}},4,40,{4},jj,singlePot(7161,2541,1),true);
    assert(turn.power.exact&&turn.tail.exact&&turn.hi<0);
    // KJs: sensitivity to different value-selection shapes, not a blanket
    // ban on KJ or a decision chosen from whether the historical hand lost.
    const std::vector<Observation> kj{{3,0,5,430,160,100,20,4910,3},
        {3,1,2,0,940,0,0,4480,2},{3,1,3,470,1410,470,0,4480,2},
        {3,2,2,0,1880,0,0,4010,2},{3,2,3,940,2820,940,0,4010,2},
        {3,3,4,2326,3760,0,0,3070,2}};
    auto river=replay("KJs river",35,37,Board{{28,48,18,32,15}},5,40,{3},kj,singlePot(6086,2326,1),true);
    assert(river.power.ev>0&&river.tail.ev<0);
    // KJo side-pot decision: actual public opponent history and
    // reconstructed cumulative contributions before the calls.
    const std::vector<Observation> kju{{1,0,6,5588,2560,1600,0,5588,4},
        {2,0,6,7234,8148,5588,320,7234,4},{5,0,3,6914,15382,7554,640,9422,4},
        {5,1,2,0,28250,0,0,2508,4},{5,2,6,2508,28250,0,0,2508,4}};
    const auto prePlan=tierPlan(22296,5954,0,{{0,1600},{1,5588},{2,7554},{5,7554}},{1,2,5});
    assert(prePlan.valid);
    replay("KJo preflop",50,9,empty,0,640,{1,2,5},{kju.begin(),kju.begin()+3},prePlan,true);
    auto full=kju;
    full.push_back({0,0,5,1600,960,640,0,10317,4});
    full.push_back({0,0,3,5954,22296,7554,1600,8717,4});
    const auto turnPlan=journalPlan(30758,2508,0,2,full,{{0,0},{1,0},{2,0},{5,2508}},{1,2,5});
    assert(turnPlan.valid&&turnPlan.tiers.size()==3);
    assert(turnPlan.tiers[0].amount==22352&&turnPlan.tiers[1].amount==5898&&turnPlan.tiers[2].amount==5016);
    replay("KJo turn",50,9,Board{{39,23,14,20,-1}},4,640,{1,2,5},kju,turnPlan,true);
    // Preserve V6 pocket kings and queens; old fraction-of-stack cutoffs
    // are not reintroduced. Use the actual public action snapshots.
    const std::vector<Observation> kk{{8,0,5,240,120,80,0,4640,8},
        {5,0,3,640,1080,720,80,8082,3},{8,0,5,1920,1720,720,240,4400,3}};
    replay("KK previous regression",11,50,empty,0,80,{5,8},kk,singlePot(3640,1440,2),false);
    const std::vector<Observation> qq{{4,0,5,210,120,80,0,3494,9},
        {7,0,5,735,330,210,0,4415,7},{4,0,6,3284,3270,2205,210,3284,3},
        {7,0,3,2759,6554,3494,735,3680,3}};
    replay("QQ previous regression",49,10,empty,0,80,{4,7},qq,
        tierPlan(9313,1289,0,{{0,2205},{4,3494},{7,3494},{2,40},{3,80}},{4,7}),false);
    // No action labels => no inferred authorization or new veto.
    assert(!assess(11,50,empty,0,{8},{},80,singlePot(3640,1440,1),5000,123,{}).valid);
    assert(applyCall(call,{},true,11,50).action==Action::Call);
    assert(applyCall(call,turn,true,12,25).action==Action::Call);
    assert(applyCall({Action::Fold,0,true,"old"},turn,false,9,22).action==Action::Fold);
    // No river response observations: do not manufacture a bet EV.
    assert(!probe(35,37,Board{{28,48,18,32,15}},5,3,kj,40,3760,1880,5000,123,{}).valid);
    auto pr=probe(35,37,Board{{28,48,18,32,-1}},4,3,{kj.begin(),kj.begin()+4},40,1880,940,5000,123,{});
    assert(pr.valid&&pr.responses==17&&pr.lo<=pr.hi);
    std::cout<<"V8 empirical range, side-pot, regret and previous-incident regressions passed\n";
}
