// Local experiment, public cards only. Integer rank: category then five
// rank digits (base 15). Independent evaluator, checked against known hands.
// Likelihood weights are explicit provisional scenarios, NOT learned GTO.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>
namespace KibitzerBoard {
using Cards=std::array<int,7>;
inline int rank(int c){return c%13+2;}
inline bool valid(int c){return c>=0&&c<52;}
inline int encode(int cat,int a=0,int b=0,int c=0,int d=0,int e=0){
    return (((((cat*15+a)*15+b)*15+c)*15+d)*15+e);
}
inline int category(int v){return v/759375;}
inline int straight(const std::array<int,15>& n){
    for(int t=14;t>=5;--t){
        bool ok=true;
        for(int i=t-4;i<=t;++i) if(!n[i==1?14:i])ok=false;
        if(ok)return t;
    }
    return 0;
}
inline int value(const Cards& c,int n){
    if(n<5||n>7)return -1;
    std::array<bool,52> seen{};
    std::array<int,15> r{};
    std::array<std::array<int,15>,4> s{};
    std::array<int,4> count{};
    for(int i=0;i<n;++i){
        const int x=c[i];
        if(!valid(x)||seen[x])return -1;
        seen[x]=true;++r[rank(x)];++s[x/13][rank(x)];++count[x/13];
    }
    for(int i=0;i<4;++i)if(count[i]>=5){
        const int t=straight(s[i]);if(t)return encode(8,t);
    }
    int four=0,three=0,pair=0;
    for(int i=14;i>=2;--i){
        if(r[i]==4)four=i;
        if(!three&&r[i]>=3)three=i;
    }
    if(four){for(int i=14;i>=2;--i)if(i!=four&&r[i])return encode(7,four,i);}
    if(three){for(int i=14;i>=2;--i)if(i!=three&&r[i]>=2){pair=i;break;}}
    if(three&&pair)return encode(6,three,pair);
    for(int i=0;i<4;++i)if(count[i]>=5){
        std::array<int,5> k{};int j=0;
        for(int t=14;t>=2&&j<5;--t)if(s[i][t])k[j++]=t;
        return encode(5,k[0],k[1],k[2],k[3],k[4]);
    }
    const int t=straight(r);if(t)return encode(4,t);
    std::array<int,5> k{};int j=0;
    if(three){
        for(int i=14;i>=2&&j<2;--i)if(i!=three&&r[i])k[j++]=i;
        return encode(3,three,k[0],k[1]);
    }
    int p1=0,p2=0;
    for(int i=14;i>=2;--i)if(r[i]>=2){if(!p1)p1=i;else if(!p2)p2=i;}
    if(p2){for(int i=14;i>=2;--i)if(i!=p1&&i!=p2&&r[i])return encode(2,p1,p2,i);}
    if(p1){
        for(int i=14;i>=2&&j<3;--i)if(i!=p1&&r[i])k[j++]=i;
        return encode(1,p1,k[0],k[1],k[2]);
    }
    for(int i=14;i>=2&&j<5;--i)if(r[i])k[j++]=i;
    return encode(0,k[0],k[1],k[2],k[3],k[4]);
}
struct State {
    int h0=-1,h1=-1,n=0;
    std::array<int,5> board{{-1,-1,-1,-1,-1}};
};
inline Cards cards(const State& s,int a,int b){
    Cards c{{a,b,-1,-1,-1,-1,-1}};
    for(int i=0;i<s.n;++i)c[i+2]=s.board[i];
    return c;
}
inline bool validState(const State& s){
    if(s.n!=0&&(s.n<3||s.n>5))return false;
    std::array<bool,52> seen{};
    auto c=cards(s,s.h0,s.h1);
    for(int i=0;i<s.n+2;++i){if(!valid(c[i])||seen[c[i]])return false;seen[c[i]]=true;}
    return true;
}
inline bool privatePair(const State& s,int a,int b){
    if(rank(a)==rank(b))return true;
    for(int i=0;i<s.n;++i)if(rank(s.board[i])==rank(a)||rank(s.board[i])==rank(b))return true;
    return false;
}
// Compare the made component, not just the kicker. The board can itself
// supply two pairs, trips, a straight, a flush or a full house.
inline int madeKey(int v){
    const int cat=category(v);
    if(cat==0)return 0;
    if(cat==5)return v; // all five ranks are part of a flush
    const int digits=cat==2||cat==6?3375:50625;
    return v/digits*digits;
}
inline int boardKey(const State& s){
    if(s.n==5){
        Cards c{};std::copy(s.board.begin(),s.board.end(),c.begin());
        return madeKey(value(c,5));
    }
    std::array<int,15> n{};
    for(int i=0;i<s.n;++i)++n[rank(s.board[i])];
    int p1=0,p2=0;
    for(int i=14;i>=2;--i){
        if(n[i]==4)return encode(7,i);
        if(n[i]==3)return encode(3,i);
        if(n[i]==2){if(!p1)p1=i;else p2=i;}
    }
    if(p2)return encode(2,p1,p2);
    return p1?encode(1,p1):0;
}
struct Texture{
    bool ready=false,paired=false,privateMade=false,boardStraight=false,boardFlush=false;
    int category=0,maxSuit=0,pairedRanks=0,tripledRanks=0,straightOrFlushOuts=0;
};
inline Texture analyze(const State& s){
    Texture t;if(!validState(s)||s.n<3)return t;
    t.ready=true;std::array<int,15> ranks{};std::array<int,4> suits{};
    for(int i=0;i<s.n;++i){++ranks[rank(s.board[i])];t.maxSuit=std::max(t.maxSuit,++suits[s.board[i]/13]);}
    for(int n:ranks){if(n>=2)++t.pairedRanks;if(n>=3)++t.tripledRanks;}
    t.paired=t.pairedRanks>0;t.boardStraight=straight(ranks)>0;t.boardFlush=t.maxSuit>=5;
    auto c=cards(s,s.h0,s.h1);
    int v=value(c,s.n+2);t.category=category(v);
    t.privateMade=madeKey(v)>boardKey(s);
    // On flop these are immediate turn improvements, not two-card/backdoor
    // odds; on turn they are nominal river completions.
    if(s.n<5&&t.category<4){
        for(int x=0;x<52;++x){
            if(std::find(c.begin(),c.begin()+s.n+2,x)!=c.begin()+s.n+2)continue;
            c[s.n+2]=x;int cat=category(value(c,s.n+3));
            if(cat==4||cat==5||cat==8)++t.straightOrFlushOuts;
        }
    }
    return t;
}
// Evidence is a priced decision against a single identifiable opponent;
// board is the board AT that decision. This is not a full action trace.
struct Evidence {State state; double betFraction=0;};
inline double likelihood(const Evidence& e,int a,int b,int scenario){
    const State& s=e.state;
    int hi=std::max(rank(a),rank(b)),lo=std::min(rank(a),rank(b));
    double strength=0;
    if(s.n==0){
        strength=(hi==lo&&hi>=9)||(hi==14&&lo>=11)||(hi==13&&lo==12)?1:
                 (hi==lo||hi>=13||(a/13==b/13&&hi>=10))?.55:.12;
    }else{
        int cat=category(value(cards(s,a,b),s.n+2));
        strength=cat>=4?1:cat==3&&privatePair(s,a,b)?1:
                 privatePair(s,a,b)?.68:.12;
        if(cat>=4&&s.n==5){Cards board{};std::copy(s.board.begin(),s.board.end(),board.begin());
            if(value(cards(s,a,b),7)==value(board,5))strength=.12;
        }
    }
    // Nonzero bluff floor; scenarios differ in severity. Large bets favor
    // strength more than small bets. No claim of empirical calibration.
    const double floor= scenario==0?.06:scenario==1?.18:.40;
    const double intensity=std::clamp(2*e.betFraction,.15,1.0);
    return 1-intensity*(1-(floor+(1-floor)*strength));
}
struct Estimate{
    bool ready=false;int samples=0;
    std::array<double,3> equity{},effectiveSamples{},winningDrawOuts{};
    std::array<bool,52> nominalOut{};
    std::array<std::array<double,52>,3> outWinProbability{},outCardProbability{};
};
inline Estimate estimate(const State& s,const std::vector<Evidence>& history,int requested,std::uint32_t seed){
    Estimate out;if(!validState(s)||requested<1)return out;
    auto hero=cards(s,s.h0,s.h1);std::vector<int> deck;
    for(int x=0;x<52;++x)if(std::find(hero.begin(),hero.begin()+s.n+2,x)==hero.begin()+s.n+2)deck.push_back(x);
    std::array<double,3> sum{},sq{},wins{},drawWins{},drawMass{};
    std::array<std::array<double,52>,3> cardMass{},cardWinMass{};
    std::mt19937 rng(seed);
    const int heroCurrent=s.n>=3?category(value(hero,s.n+2)):0;
    auto weights=[&](int a,int b){
        std::array<double,3> w{{1,1,1}};
        for(int q=0;q<3;++q)for(const auto& e:history)w[q]*=likelihood(e,a,b,q);
        return w;
    };
    auto deal=[&](int a,int b,int x,int y,int z,int u,int v,const std::array<double,3>& weight){
        Cards hc=hero,vc=cards(s,a,b);
        const std::array<int,5> future{{x,y,z,u,v}};
        for(int k=s.n;k<5;++k)hc[k+2]=vc[k+2]=future[k-s.n];
        const int hv=value(hc,7),vv=value(vc,7);
        const double result=hv>vv?1:hv==vv?.5:0;
        const bool draw=s.n==4&&(category(hv)==4||category(hv)==5||category(hv)==8)&&heroCurrent<4;
        for(int q=0;q<3;++q){
            const double w=weight[q];
            sum[q]+=w;sq[q]+=w*w;wins[q]+=w*result;
            if(draw){drawWins[q]+=w*result;out.nominalOut[x]=true;}
            if(s.n==4){cardMass[q][x]+=w;cardWinMass[q][x]+=w*result;}
            drawMass[q]+=w;
        }
        ++out.samples;
    };
    if(s.n>=4){
        for(int a:deck)for(int b:deck)if(a<b){
            const auto w=weights(a,b);
            if(s.n==5)deal(a,b,-1,-1,-1,-1,-1,w);
            else for(int x:deck)if(x!=a&&x!=b)deal(a,b,x,-1,-1,-1,-1,w);
        }
    }else{
        const int needed=7-s.n;
        for(int i=0;i<requested;++i){
            for(int k=0;k<needed;++k){std::uniform_int_distribution<int> pick(k,int(deck.size())-1);std::swap(deck[k],deck[pick(rng)]);}
            std::array<int,5> f{{-1,-1,-1,-1,-1}};
            for(int k=0;k<5-s.n;++k)f[k]=deck[k+2];
            deal(deck[0],deck[1],f[0],f[1],f[2],f[3],f[4],weights(deck[0],deck[1]));
        }
    }
    out.ready=true;
    for(int q=0;q<3;++q){
        if(sum[q]<=0||sq[q]<=0){out.ready=false;return out;}
        out.equity[q]=wins[q]/sum[q];out.effectiveSamples[q]=sum[q]*sum[q]/sq[q];
        // Equivalent winning outs under the specified HU range, including ties.
        if(s.n==4){
            out.winningDrawOuts[q]=44*drawWins[q]/drawMass[q];
            for(int x:deck)if(cardMass[q][x]>0){
                out.outWinProbability[q][x]=cardWinMass[q][x]/cardMass[q][x];
                out.outCardProbability[q][x]=cardMass[q][x]/sum[q];
            }
        }
        if(s.n<4&&out.effectiveSamples[q]<200)out.ready=false;
    }
    return out;
}
// Collision-free joint proposal over all physical cards. Importance weights
// condition on the recorded history for EACH still-eligible opponent.
// Folded hands and strategic dependencies are not modeled. Reject inadequate
// effective sample size rather than silently substitute random equity.
inline Estimate estimateMultiway(const State& s,
        const std::vector<std::vector<Evidence>>& histories,int requested,std::uint32_t seed){
    Estimate out;
    const int opponents=static_cast<int>(histories.size());
    if(!validState(s)||opponents<2||opponents>9||requested<1000)return out;
    auto hero=cards(s,s.h0,s.h1);std::vector<int> deck;
    for(int x=0;x<52;++x)if(std::find(hero.begin(),hero.begin()+s.n+2,x)==hero.begin()+s.n+2)deck.push_back(x);
    const int needed=2*opponents+5-s.n;
    if(needed>static_cast<int>(deck.size()))return out;
    // Precompute combination weights; known-card removal happens in the
    // joint deal, not by independently drawing potentially colliding hands.
    std::vector<std::array<std::array<double,3>,1326>> weights(opponents);
    auto index=[](int a,int b){if(a>b)std::swap(a,b);return a*(103-a)/2+b-a-1;};
    for(int j=0;j<opponents;++j)
        for(int a:deck)for(int b:deck)if(a<b){
            auto& w=weights[j][index(a,b)];w={{1,1,1}};
            for(int q=0;q<3;++q)for(const auto& e:histories[j])w[q]*=likelihood(e,a,b,q);
        }
    std::array<double,3> total{},squares{},wins{},drawWins{};
    std::array<std::array<double,52>,3> cardMass{},cardWin{};
    const int heroCurrent=s.n>=3?category(value(hero,s.n+2)):0;
    std::mt19937 rng(seed);
    for(int i=0;i<requested;++i){
        for(int k=0;k<needed;++k){
            std::uniform_int_distribution<int> pick(k,static_cast<int>(deck.size())-1);
            std::swap(deck[k],deck[pick(rng)]);
        }
        auto hc=hero;
        for(int k=s.n;k<5;++k)hc[k+2]=deck[2*opponents+k-s.n];
        const int hv=value(hc,7);
        bool beaten=false;int ties=1;std::array<double,3> w{{1,1,1}};
        for(int j=0;j<opponents;++j){
            auto vc=hc;vc[0]=deck[2*j];vc[1]=deck[2*j+1];
            const int vv=value(vc,7);
            beaten|=vv>hv;if(vv==hv)++ties;
            const auto& vw=weights[j][index(vc[0],vc[1])];
            for(int q=0;q<3;++q)w[q]*=vw[q];
        }
        const double share=beaten?0:1.0/ties;
        const bool draw=s.n==4&&heroCurrent<4&&(category(hv)==4||category(hv)==5||category(hv)==8);
        const int x=s.n==4?hc[6]:-1;
        for(int q=0;q<3;++q){
            total[q]+=w[q];squares[q]+=w[q]*w[q];wins[q]+=w[q]*share;
            if(draw){drawWins[q]+=w[q]*share;out.nominalOut[x]=true;}
            if(s.n==4){cardMass[q][x]+=w[q];cardWin[q][x]+=w[q]*share;}
        }
    }
    out.samples=requested;out.ready=true;
    for(int q=0;q<3;++q){
        if(total[q]<=0||squares[q]<=0){out.ready=false;return out;}
        out.equity[q]=wins[q]/total[q];
        out.effectiveSamples[q]=total[q]*total[q]/squares[q];
        if(out.effectiveSamples[q]<std::max(200.0,.05*requested))out.ready=false;
        if(s.n==4){
            // Scale to number of river candidates after all sampled hole cards.
            out.winningDrawOuts[q]=(46-2*opponents)*drawWins[q]/total[q];
            for(int x:deck)if(cardMass[q][x]>0){
                out.outWinProbability[q][x]=cardWin[q][x]/cardMass[q][x];
                out.outCardProbability[q][x]=cardMass[q][x]/total[q];
            }
        }
    }
    return out;
}
} // namespace KibitzerBoard
