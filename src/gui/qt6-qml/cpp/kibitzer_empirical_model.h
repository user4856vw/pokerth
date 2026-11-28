// V8: frequency-constrained VALUE-SELECTIVE hypotheses, not identified
// opponent card distributions. Only public cards/action journal are inputs.
// See docs/kibitzer-v8-model.md for provenance, censoring and policy limits.
#pragma once
#include "kibitzer_strategy_p0.h"
#include <map>
#include <set>
#include <numeric>
#include <utility>
#include <limits>

namespace KibitzerEmpirical {
using namespace KibitzerOpponent;
using KibitzerAutoplay::Action;
using KibitzerAutoplay::Decision;

// Fitted ONLY on the captured public actions of V6 sessions 17/18.
// Denominators for aggression exclude stacks unable to exceed call price.
// Continuation includes calls and aggression. See reproducible extraction
// script; no card labels or game outcomes are used to estimate these rates.
struct Counts {int raises,raiseOpportunities,continues,opportunities;};
inline constexpr std::array<Counts,9> training{{
    {703,4976,709,5008},{99,1889,239,1963},{7,27,24,27},
    {19,85,19,85},{0,18,14,18},{17,72,17,72},
    {0,17,14,17},{17,51,17,51},{0,0,0,0}
}};
inline int context(const Observation& a,int bb,bool previouslyAggressive){
    if(a.street<0||a.street>3||bb<=0)return -1;
    if(a.street==0)return a.highestBefore<=bb?0:previouslyAggressive?2:1;
    return 3+2*(a.street-1)+(a.highestBefore>a.committedBefore?1:0);
}
struct Rate {bool valid=false;double point=0,lo=0,hi=1;};
// Wilson interval: 1.96 is the conventional normal 95% quantile, not a
// poker parameter. Correlated/censored observations invalidate nominal
// population coverage; the interval is a sample-size sensitivity device.
inline Rate rate(int k,int n){
    if(n<=0||k<0||k>n)return {};
    constexpr double z=1.96;
    const double p=double(k)/n,den=1+z*z/n;
    const double center=(p+z*z/(2*n))/den;
    const double half=z*std::sqrt(p*(1-p)/n+z*z/(4.0*n*n))/den;
    return {true,p,std::max(0.0,center-half),std::min(1.0,center+half)};
}
// Independent 100,000-deal HU uniform-opponent reference per 169 classes.
// Rank 2..A, diagonal pairs, upper triangle suited, lower offsuit.
// Integer units 1/10000, only an ordering proxy; this is NOT equity against
// the current opponent. Reproduce with scripts/kibitzer_v8_rank_table.py.
inline constexpr int equityRank[13][13]={
{5025,3558,3690,3779,3767,3816,4034,4245,4472,4724,5029,5304,5725},
{3235,5385,3862,3984,3961,3984,4090,4307,4584,4781,5122,5386,5824},
{3358,3497,5702,4145,4146,4153,4272,4382,4676,4902,5206,5521,5910},
{3426,3627,3827,6027,4291,4388,4447,4570,4716,5001,5282,5562,5982},
{3407,3584,3788,4002,6311,4537,4633,4741,4899,5058,5385,5665,6003},
{3457,3638,3860,4032,4246,6619,4766,4920,5065,5232,5420,5751,6088},
{3686,3729,3926,4152,4353,4501,6910,5084,5264,5415,5615,5836,6214},
{3918,3991,4054,4258,4465,4623,4812,7206,5403,5571,5796,6004,6280},
{4156,4254,4342,4388,4586,4780,4989,5143,7487,5758,5945,6172,6478},
{4441,4523,4606,4713,4772,4977,5147,5330,5544,7746,6039,6277,6542},
{4713,4839,4902,5020,5092,5146,5344,5555,5732,5834,8008,6349,6633},
{5026,5147,5246,5337,5416,5531,5598,5751,5962,6059,6128,8225,6710},
{5508,5582,5671,5778,5753,5881,5961,6070,6267,6374,6444,6547,8513}
};
inline unsigned preflopRank(int a,int b){
    if(!KibitzerBoard::card(a)||!KibitzerBoard::card(b)||a==b)return 0;
    const int lo=std::min(a%13,b%13),hi=std::max(a%13,b%13);
    return static_cast<unsigned>(a/13==b/13?equityRank[lo][hi]:equityRank[hi][lo]);
}
enum class Shape { Power, TopTail };
inline std::vector<unsigned> scores(const Range& r,const Board& board,int n){
    std::vector<unsigned> s;s.reserve(r.size());
    for(const auto& c:r)s.push_back(n==0?preflopRank(c.a,c.b):KibitzerBoard::value(c.a,c.b,board,n));
    return s;
}
// A randomized weighted rank R is uniform on [0,1] even with ties.
// Power: L(R)=R^(1/q-1), integral q, max L=1.
// TopTail: L(R)=1 on the top q probability mass, 0 elsewhere.
// Integrate over each tied mass interval instead of arbitrarily ordering
// equal-valued combinations. Both likelihoods have marginal mass q.
// A call is coarsened to "continued"; no fabricated call-vs-raise split.
inline bool select(Range& r,const std::vector<unsigned>& s,double q,Shape shape){
    if(r.empty()||r.size()!=s.size()||!std::isfinite(q)||q<=0||q>1||!normalize(r))return false;
    if(q==1)return true;
    std::vector<size_t> order(r.size());std::iota(order.begin(),order.end(),size_t{0});
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return s[a]<s[b];});
    double low=0;
    for(size_t i=0;i<order.size();){
        size_t j=i+1;while(j<order.size()&&s[order[j]]==s[order[i]])++j;
        double mass=0;for(size_t k=i;k<j;++k)mass+=r[order[k]].weight;
        const double high=std::min(1.0,low+mass);
        const double selected=shape==Shape::TopTail?
            std::max(0.0,high-std::max(low,1-q)):
            q*(std::pow(high,1/q)-std::pow(low,1/q));
        const double likelihood=mass>0?std::clamp(selected/mass,0.0,1.0):0.0;
        for(size_t k=i;k<j;++k)r[order[k]].weight*=likelihood;
        low=high;i=j;
    }
    return normalize(r);
}
struct Posterior {Range range;int informative=0;};
inline Posterior posterior(int a,int b,const Board& board,int n,int seat,
                           const std::vector<Observation>& history,int bb,Shape shape){
    Posterior p{uniform(a,b,board,n),0};
    if(p.range.empty()||!normalize(p.range)||bb<=0)return {};
    bool previous=false;
    for(const auto& x:history){
        if(x.seat!=seat||x.street<0||x.street>3||boardCount(x.street)>n)continue;
        const bool ag=aggression(x),cont=(x.action==3||x.action==6)&&!ag;
        const int c=context(x,bb,previous);
        if(x.street==0&&ag)previous=true;
        if(c<0||(!ag&&!cont)||(!ag&&c==0))continue;
        const auto z=training[static_cast<size_t>(c)];
        const auto f=ag?rate(z.raises,z.raiseOpportunities):rate(z.continues,z.opportunities);
        // Use the broader Wilson endpoint in BOTH value-driven scenarios,
        // not an arbitrary weak-hand probability floor.
        if(!f.valid||f.hi>=1)continue;
        if(!select(p.range,scores(p.range,board,boardCount(x.street)),f.hi,shape))return {};
        ++p.informative;
    }
    return p;
}

struct Tier {long long amount=0;uint64_t mask=0;};
struct Plan {bool valid=false;int price=0;std::vector<Tier> tiers;};
inline Plan singlePot(int pot,int price,size_t opponents){
    if(pot<=0||price<=0||opponents==0||opponents>=64)return {};
    return {true,price,{{static_cast<long long>(pot)+price,(1ULL<<opponents)-1}}};
}
// Contributions include dead/folded chips, but only active villains can
// win a tier. Cap at hero's FINAL contribution; higher tiers are not ours.
inline Plan tierPlan(int pot,int price,int hero,const std::map<int,long long>& before,
                     const std::vector<int>& villains){
    if(pot<=0||price<=0||villains.empty()||villains.size()>=64||!before.count(hero))return {};
    long long total=0;
    for(auto [id,x]:before){(void)id;if(x<0||x>pot)return {};total+=x;}
    if(total!=pot)return {};
    std::set<int> unique;
    const long long cap=before.at(hero)+price;
    bool matched=false;
    for(int id:villains){
        if(id==hero||!unique.insert(id).second||!before.count(id))return {};
        matched=matched||before.at(id)>=cap;
    }
    // A genuine incremental call cannot exceed every active opposing stake.
    if(!matched)return {};
    auto after=before;after[hero]=cap;
    std::set<long long> levels{cap};
    for(auto [id,x]:after){(void)id;if(x>0&&x<cap)levels.insert(x);}
    Plan out{true,price,{}};long long previous=0;
    for(long long l:levels){
        long long count=0;for(auto [id,x]:after){(void)id;count+=x>=l;}
        uint64_t mask=0;
        for(size_t i=0;i<villains.size();++i)if(after.at(villains[i])>=l)mask|=1ULL<<i;
        const long long amount=(l-previous)*count;
        if(amount<=0)return {};
        out.tiers.push_back({amount,mask});previous=l;
    }
    return out;
}
// Journal stores each actor's cumulative street commitment before action.
// Previous streets use the maximum observed post-action commitment;
// current street uses the live engine sets. Reconcile to pot or abstain.
inline Plan journalPlan(int pot,int price,int hero,int street,
                        const std::vector<Observation>& allHistory,
                        const std::vector<std::pair<int,int>>& current,
                        const std::vector<int>& villains){
    if(street<0||street>3)return {};
    std::map<int,std::array<long long,4>> ledger;
    for(const auto& a:allHistory){
        if(a.street<0||a.street>street||a.committedBefore<0||a.paid<0)return {};
        auto& v=ledger[a.seat][static_cast<size_t>(a.street)];
        v=std::max(v,static_cast<long long>(a.committedBefore)+a.paid);
    }
    std::set<int> seen;
    for(auto [id,set]:current){
        if(set<0||!seen.insert(id).second)return {};
        ledger[id][static_cast<size_t>(street)]=set;
    }
    if(!seen.count(hero))return {};
    std::map<int,long long> totals;
    for(const auto& [id,byStreet]:ledger)
        totals[id]=std::accumulate(byStreet.begin(),byStreet.end(),0LL);
    return tierPlan(pot,price,hero,totals,villains);
}
struct Projection {
    bool valid=false,exact=false;int samples=0;
    double ev=0,standardError=0,globalEquity=0;
};
inline double payoff(unsigned own,const std::vector<unsigned>& other,const Plan& plan){
    double amount=0;
    for(const auto& t:plan.tiers){
        bool beaten=false;int ties=0;
        for(size_t i=0;i<other.size();++i)if(t.mask&(1ULL<<i)){
            if(other[i]>own){beaten=true;break;}
            ties+=other[i]==own;
        }
        if(!beaten)amount+=double(t.amount)/(1+ties);
    }
    return amount-plan.price;
}
inline Projection project(int a,int b,const Board& board,int n,const std::vector<Range>& ranges,
                           const Plan& plan,int samples,uint32_t seed,const KibitzerBoard::Analysis& analysis){
    Projection out;
    if(!plan.valid||plan.price<=0||plan.tiers.empty()||ranges.empty()||ranges.size()>=64||
       samples<=0||!KibitzerBoard::valid(a,b,board,n))return out;
    for(const auto& t:plan.tiers)if(t.amount<=0||(t.mask>>ranges.size())!=0)return {};
    for(const auto& r:ranges)if(r.empty())return {};
    if(ranges.size()==1&&(n==4||n==5)){
        const auto e=exactHU(a,b,board,n,ranges.front(),analysis);
        if(!e.valid)return {};
        double contested=0,uncontested=0;
        for(const auto& t:plan.tiers)(t.mask?contested:uncontested)+=t.amount;
        return {true,true,e.samples,uncontested+contested*e.equity-plan.price,0,e.equity};
    }
    std::vector<std::discrete_distribution<size_t>> distributions;
    for(const auto& r:ranges){
        std::vector<double> w;w.reserve(r.size());
        for(const auto& c:r)w.push_back(c.weight);
        distributions.emplace_back(w.begin(),w.end());
    }
    const auto known=KibitzerBoard::mask(a,b,board,n);
    std::mt19937 rng(seed);std::uniform_int_distribution<int> card(0,51);
    std::vector<Combo> picked(ranges.size(),{0,0,0});
    std::vector<unsigned> values(ranges.size());
    double mean=0,m2=0,share=0;
    // Same computational budget as the inherited Monte Carlo: reject
    // incompatible joint assignments, never resample one seat independently.
    for(long long attempt=0;attempt<40LL*samples&&out.samples<samples;++attempt){
        uint64_t used=known;bool ok=true;
        for(size_t i=0;i<ranges.size();++i){
            picked[i]=ranges[i][distributions[i](rng)];
            const auto bits=(1ULL<<picked[i].a)|(1ULL<<picked[i].b);
            if(used&bits){ok=false;break;}used|=bits;
        }
        if(!ok)continue;
        Board next=board;
        for(int i=n;i<5;++i){
            int k;do{k=card(rng);}while(used&(1ULL<<k));
            used|=1ULL<<k;next[static_cast<size_t>(i)]=k;
        }
        const unsigned own=KibitzerBoard::value(a,b,next,5);
        bool beaten=false;int ties=0;
        for(size_t i=0;i<picked.size();++i){
            values[i]=KibitzerBoard::value(picked[i].a,picked[i].b,next,5);
            beaten=beaten||values[i]>own;ties+=values[i]==own;
        }
        share+=beaten?0.0:1.0/(1+ties);
        const double v=payoff(own,values,plan);
        ++out.samples;const double delta=v-mean;mean+=delta/out.samples;m2+=delta*(v-mean);
    }
    // Inherited minimum sample requirement, not a strategy threshold.
    if(out.samples<std::min(samples,1000))return out;
    out.valid=true;out.ev=mean;out.globalEquity=share/out.samples;
    if(out.samples>1)out.standardError=std::sqrt(std::max(0.0,m2/(out.samples-1)/out.samples));
    return out;
}
struct Assessment {
    bool valid=false;int informative=0;Projection power,tail;
    double lo=0,hi=0,regretCall=0,regretFold=0;
};
inline Assessment assess(int a,int b,const Board& board,int n,const std::vector<int>& villains,
                         const std::vector<Observation>& history,int bb,const Plan& plan,
                         int samples,uint32_t seed,const KibitzerBoard::Analysis& analysis){
    Assessment x;if(!plan.valid||villains.empty())return x;
    auto run=[&](Shape s,int& count){
        std::vector<Range> ranges;
        for(int id:villains){
            auto p=posterior(a,b,board,n,id,history,bb,s);count+=p.informative;
            if(p.range.empty())return Projection{};
            ranges.push_back(std::move(p.range));
        }
        if(!count)return Projection{};
        return project(a,b,board,n,ranges,plan,samples,seed,analysis);
    };
    int powerCount=0,tailCount=0;
    x.power=run(Shape::Power,powerCount);x.tail=run(Shape::TopTail,tailCount);
    x.informative=std::min(powerCount,tailCount);
    x.valid=x.power.valid&&x.tail.valid;
    if(x.valid){
        // Worst regret over the two scenarios and their sampling envelopes.
        // This is a declared robust decision criterion, not GTO/ICM.
        x.lo=std::min(x.power.ev-1.96*x.power.standardError,x.tail.ev-1.96*x.tail.standardError);
        x.hi=std::max(x.power.ev+1.96*x.power.standardError,x.tail.ev+1.96*x.tail.standardError);
        x.regretCall=std::max(0.0,-x.lo);x.regretFold=std::max(0.0,x.hi);
    }
    return x;
}
inline Decision applyCall(Decision v7,const Assessment& x,bool preflop,int a,int b){
    if(v7.action!=Action::Call||!x.valid)return v7;
    // Retain original AA invariant; never interfere with a free check,
    // manual ranges, or a separately modeled raise. No new call promotions.
    if(preflop&&a>=0&&b>=0&&a%13==12&&b%13==12)return v7;
    if(x.regretCall>x.regretFold)
        return {Action::Fold,0,true,x.hi<0?
            "v8-value-selective-negative-showdown-ev":
            "v8-value-selective-minimax-regret-fold"};
    return v7;
}

// Response-aware diagnostic for a single proposed heads-up value probe.
// All scenarios assume check-down after a call; no future action tree.
// Captured response frequencies are especially censored (folds can end
// the hand without another hero observation), hence DIAGNOSTIC ONLY.
struct Probe {bool valid=false;int responses=0;double lo=0,hi=0,continueEqLo=0,continueEqHi=0;};
inline Probe probe(int a,int b,const Board& board,int n,int villain,
                   const std::vector<Observation>& history,int bb,int pot,int bet,
                   int samples,uint32_t seed,const KibitzerBoard::Analysis& analysis){
    Probe out;if(n<3||n>5||pot<=0||bet<=0)return out;
    const auto c=training[static_cast<size_t>(4+2*(n-3))];
    out.responses=c.opportunities;
    const auto q=rate(c.continues,c.opportunities),r=rate(c.raises,c.raiseOpportunities);
    if(!q.valid||!r.valid)return out;
    double lo=std::numeric_limits<double>::infinity(),hi=-lo,elo=lo,ehi=hi;
    for(Shape shape:{Shape::Power,Shape::TopTail}){
        auto post=posterior(a,b,board,n,villain,history,bb,shape);
        if(post.range.empty())return out;
        auto eq=[&](const Range& range){
            return n>=4?exactHU(a,b,board,n,range,analysis):
                monteCarlo(a,b,board,n,{range},samples,seed);
        };
        const auto base=eq(post.range);if(!base.valid)return out;
        for(double f:{q.lo,q.point,q.hi}){
            if(f<=0)continue;
            auto cont=post.range;
            if(!select(cont,scores(cont,board,n),f,shape))return out;
            const auto e=eq(cont);if(!e.valid)return out;
            const double delta=(1-f)*pot+f*(e.equity*(double(pot)+2.0*bet)-bet)-base.equity*pot;
            const double se=f*(double(pot)+2.0*bet)*e.standardError+pot*base.standardError;
            // A re-raise followed by hero folding can cost at most P+2B
            // relative to the assumed called branch. Rate is an endpoint
            // sensitivity, not a guaranteed bound on actual future behavior.
            lo=std::min(lo,delta-1.96*se-r.hi*(double(pot)+2.0*bet));
            hi=std::max(hi,delta+1.96*se);
            elo=std::min(elo,e.equity);ehi=std::max(ehi,e.equity);
        }
    }
    if(std::isfinite(lo)&&std::isfinite(hi)){
        out.valid=true;out.lo=lo;out.hi=hi;out.continueEqLo=elo;out.continueEqHi=ehi;
    }
    return out;
}
} // namespace KibitzerEmpirical
