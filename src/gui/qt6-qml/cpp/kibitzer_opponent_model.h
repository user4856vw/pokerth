// Explicit provisional action likelihood model, not a calibrated solver.
// Condition independent seat priors on public actions; joint deals reject
// card collisions. Never inspect engine bot strategy or concealed cards.
#pragma once
#include "kibitzer_board_analysis.h"
#include "kibitzer_preflop_ranges.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>
#include <cstdint>

namespace KibitzerOpponent {
using KibitzerBoard::Board;
struct Observation {
    int seat=0,street=0,action=0,paid=0,potBefore=0;
    int highestBefore=0,committedBefore=0,stackBefore=0,players=0;
};
struct Combo {
    int a,b;
    double weight=1.0;
};
using Range=std::vector<Combo>;
inline int boardCount(int street){return street==0?0:street+2;}
inline bool aggression(const Observation& a) {
    return a.action==4||a.action==5||
        (a.action==6 && static_cast<int64_t>(a.committedBefore)+a.paid>a.highestBefore);
}
inline Range uniform(int a,int b,const Board& board,int n) {
    Range r;
    if(!KibitzerBoard::valid(a,b,board,n))return r;
    const auto known=KibitzerBoard::mask(a,b,board,n);
    for(int x=0;x<51;++x)if(!(known&(1ULL<<x)))
        for(int y=x+1;y<52;++y)if(!(known&(1ULL<<y)))r.push_back({x,y,1.0});
    return r;
}
inline bool normalize(Range& r) {
    double sum=0;for(const auto& c:r)sum+=c.weight;
    if(!std::isfinite(sum)||sum<=0)return false;
    for(auto& c:r)c.weight/=sum;
    return true;
}
inline std::vector<double> strengths(const Range& r,const Board& board,int n) {
    std::vector<double> v;
    v.reserve(r.size());
    for(const auto& c:r)v.push_back(n==0?
        KibitzerPreflopRanges::shortStackScore(KibitzerPreflopRanges::decode(c.a,c.b)):
        double(KibitzerBoard::value(c.a,c.b,board,n)));
    auto sorted=v;std::sort(sorted.begin(),sorted.end());
    for(double& x:v) {
        const auto lo=std::lower_bound(sorted.begin(),sorted.end(),x)-sorted.begin();
        const auto hi=std::upper_bound(sorted.begin(),sorted.end(),x)-sorted.begin();
        x=double(lo+hi)/(2.0*sorted.size());
    }
    return v;
}
enum class Profile { Balanced, BluffHeavy, ValueHeavy };
// A smooth strength-to-action likelihood. These are engineering priors,
// NOT observed frequencies. Bluff-heavy and value-heavy profiles expose
// model sensitivity instead of treating a number as ground truth.
inline double likelihood(const Observation& a,double strength,bool draw,
                         int bb,Profile profile) {
    const bool aggressive=aggression(a);
    const bool call=a.action==3 || (a.action==6&&!aggressive);
    if(a.action!=2&&!aggressive&&!call)return 1.0;
    double pressure=double(a.paid)/std::max(1,a.potBefore);
    if(a.street==0&&bb>0)pressure=double(a.paid+a.committedBefore)/bb/3.0;
    pressure=std::clamp(pressure,0.0,3.0);
    double floor=profile==Profile::BluffHeavy?.35:profile==Profile::ValueHeavy?.055:.15;
    if(a.street==0)floor=profile==Profile::BluffHeavy?.25:profile==Profile::ValueHeavy?.035:.09;
    if(aggressive) {
        const double exponent=1.5+pressure+(a.players>=4?.5:0.0)+(a.action==5?.5:0.0);
        double signal=std::pow(strength,exponent);
        if(draw&&a.street>0&&a.street<3)signal=std::max(signal,.30);
        return floor+(1-floor)*signal;
    }
    if(call) {
        const double signal=std::max(strength,draw?.45:0.0);
        return .35+.65*std::pow(signal,1.0+pressure*.5);
    }
    // Checks are weak information: do not rule out traps.
    return 1.0-.15*strength;
}
inline Range posterior(const Range& prior,const Board& board,int n,int seat,
                       const std::vector<Observation>& history,int bb,Profile profile) {
    Range r=prior;
    for(const auto& a:history) {
        if(a.seat!=seat||a.street<0||a.street>3||a.paid<0||
            a.action<2||a.action>6)continue;
        const int k=boardCount(a.street);
        if(k>n)continue;
        const auto strength=strengths(r,board,k);
        for(size_t i=0;i<r.size();++i) {
            const auto& c=r[i];
            const bool draw=k>=3&&k<5&&KibitzerBoard::category(KibitzerBoard::value(c.a,c.b,board,k))<4&&
                KibitzerBoard::oneCardDraw(c.a,c.b,board,k);
            r[i].weight*=likelihood(a,strength[i],draw,bb,profile);
        }
        if(!normalize(r))return {};
    }
    if(!normalize(r))return {};
    return r;
}
struct Estimate {
    bool valid=false,exact=false;
    double equity=0,standardError=0,drawWinShare=0,drawProbability=0;
    int samples=0;
};
// Exact heads-up turn or river showdown share. Each opponent combination
// has a normalized posterior weight, and each remaining board card is
// equally likely conditional on that combination. Ties are half a pot.
inline Estimate exactHU(int a,int b,const Board& board,int n,const Range& range,
                        const KibitzerBoard::Analysis& analysis) {
    Estimate e;if((n!=4&&n!=5)||range.empty())return e;
    const auto known=KibitzerBoard::mask(a,b,board,n);
    double mass=0,drawMass=0,drawEquity=0;
    for(const auto& c:range) {
        const uint64_t bits=(1ULL<<c.a)|(1ULL<<c.b);
        if(bits&known)continue;
        if(n==5) {
            const auto x=KibitzerBoard::value(a,b,board,5),y=KibitzerBoard::value(c.a,c.b,board,5);
            const double share=x>y?1.0:x==y?.5:0.0;
            e.equity+=c.weight*share;mass+=c.weight;++e.samples;
        }else{
            for(int k=0;k<52;++k)if(!((known|bits)&(1ULL<<k))){
                Board next=board;next[4]=k;
                const auto x=KibitzerBoard::value(a,b,next,5),y=KibitzerBoard::value(c.a,c.b,next,5);
                const double share=x>y?1.0:x==y?.5:0.0,w=c.weight/44.0;
                e.equity+=w*share;mass+=w;++e.samples;
                if(analysis.drawOut[k]){drawMass+=w;drawEquity+=w*share;}
            }
        }
    }
    if(mass<=0)return {};
    e.valid=true;e.exact=true;e.equity/=mass;e.drawWinShare=drawEquity/mass;e.drawProbability=drawMass/mass;
    return e;
}
inline Estimate monteCarlo(int a,int b,const Board& board,int n,
                           const std::vector<Range>& ranges,int samples,uint32_t seed) {
    Estimate e;
    if(ranges.empty()||samples<1||!KibitzerBoard::valid(a,b,board,n))return e;
    std::vector<std::discrete_distribution<int>> d;
    for(const auto& r:ranges){
        if(r.empty())return e;
        std::vector<double> w;for(const auto& c:r)w.push_back(c.weight);
        d.emplace_back(w.begin(),w.end());
    }
    const auto known=KibitzerBoard::mask(a,b,board,n);
    std::mt19937 rng(seed);std::uniform_int_distribution<int> card(0,51);
    std::vector<Combo> picked(ranges.size(),{0,0,0});
    double sum=0,sum2=0;
    for(int attempt=0;attempt<samples*40&&e.samples<samples;++attempt){
        uint64_t used=known;bool ok=true;
        for(size_t i=0;i<ranges.size();++i){
            picked[i]=ranges[i][d[i](rng)];
            const auto bits=(1ULL<<picked[i].a)|(1ULL<<picked[i].b);
            if(bits&used){ok=false;break;}used|=bits;
        }
        if(!ok)continue;
        Board next=board;
        for(int i=n;i<5;++i){int k;do{k=card(rng);}while(used&(1ULL<<k));used|=1ULL<<k;next[i]=k;}
        auto h=KibitzerBoard::value(a,b,next,5);
        bool beaten=false;int ties=0;
        for(const auto& c:picked){
            auto v=KibitzerBoard::value(c.a,c.b,next,5);
            if(v>h){beaten=true;break;}ties+=v==h;
        }
        const double share=beaten?0.0:1.0/(1+ties);
        sum+=share;sum2+=share*share;++e.samples;
    }
    if(e.samples<std::min(samples,1000))return e;
    e.valid=true;e.equity=sum/e.samples;
    if(e.samples>1)e.standardError=std::sqrt(std::max(0.0,(sum2/e.samples-e.equity*e.equity)/(e.samples-1)));
    return e;
}
struct Assessment {
    bool valid=false,observedAggression=false;
    int aggressiveActions=0,aggressiveStreets=0;
    Estimate balanced,bluffHeavy,valueHeavy;
};
inline Assessment assess(int a,int b,const Board& board,int n,const std::vector<int>& seats,
                          const std::vector<Observation>& history,int bb,int samples,uint32_t seed,
                          const KibitzerBoard::Analysis& analysis) {
    Assessment out;
    if(seats.empty()||!KibitzerBoard::valid(a,b,board,n))return out;
    std::array<bool,4> streets{};
    for(const auto& x:history)if(std::find(seats.begin(),seats.end(),x.seat)!=seats.end()&&
        x.street>=0&&x.street<=3&&boardCount(x.street)<=n&&aggression(x)){
        ++out.aggressiveActions;streets[x.street]=true;
    }
    for(bool x:streets)out.aggressiveStreets+=x;
    out.observedAggression=out.aggressiveActions>0;
    if(!out.observedAggression)return out;
    const auto prior=uniform(a,b,board,n);
    auto run=[&](Profile profile){
        std::vector<Range> ranges;
        for(int seat:seats)ranges.push_back(posterior(prior,board,n,seat,history,bb,profile));
        if(ranges.size()==1&&(n==4||n==5))return exactHU(a,b,board,n,ranges.front(),analysis);
        return monteCarlo(a,b,board,n,ranges,samples,seed);
    };
    out.balanced=run(Profile::Balanced);out.bluffHeavy=run(Profile::BluffHeavy);out.valueHeavy=run(Profile::ValueHeavy);
    out.valid=out.balanced.valid&&out.bluffHeavy.valid&&out.valueHeavy.valid;
    return out;
}
} // namespace KibitzerOpponent
