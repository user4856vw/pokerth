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
struct Texture{
    bool ready=false,paired=false,privateMade=false;
    int category=0,maxSuit=0,straightOrFlushOuts=0;
};
inline Texture analyze(const State& s){
    Texture t;if(!validState(s)||s.n<3)return t;
    t.ready=true;std::array<int,15> ranks{};std::array<int,4> suits{};
    for(int i=0;i<s.n;++i){t.paired|=++ranks[rank(s.board[i])]>1;t.maxSuit=std::max(t.maxSuit,++suits[s.board[i]/13]);}
    auto c=cards(s,s.h0,s.h1);
    int v=value(c,s.n+2);t.category=category(v);
    t.privateMade=privatePair(s,s.h0,s.h1)||t.category>=4;
    if(s.n==5){Cards board{};std::copy(s.board.begin(),s.board.end(),board.begin());
        if(category(value(board,5))==t.category&&!privatePair(s,s.h0,s.h1))t.privateMade=false;
    }
    if(s.n==4&&t.category<4){
        for(int x=0;x<52;++x){
            if(std::find(c.begin(),c.begin()+6,x)!=c.begin()+6)continue;
            c[6]=x;int cat=category(value(c,7));
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
    const double intensity=std::clamp(e.betFraction,.15,1.0);
    return 1-intensity*(1-(floor+(1-floor)*strength));
}
struct Estimate{
    bool ready=false;int samples=0;
    std::array<double,3> equity{},effectiveSamples{},winningDrawOuts{};
};
inline Estimate estimate(const State& s,const std::vector<Evidence>& history,int requested,std::uint32_t seed){
    Estimate out;if(!validState(s)||requested<1)return out;
    auto hero=cards(s,s.h0,s.h1);std::vector<int> deck;
    for(int x=0;x<52;++x)if(std::find(hero.begin(),hero.begin()+s.n+2,x)==hero.begin()+s.n+2)deck.push_back(x);
    std::array<double,3> sum{},sq{},wins{},drawWins{},drawMass{};
    std::mt19937 rng(seed);
    auto deal=[&](int a,int b,int x,int y,int z,int u,int v){
        Cards hc=hero,vc=cards(s,a,b);
        const std::array<int,5> future{{x,y,z,u,v}};
        for(int k=s.n;k<5;++k)hc[k+2]=vc[k+2]=future[k-s.n];
        const int hv=value(hc,7),vv=value(vc,7);
        const double result=hv>vv?1:hv==vv?.5:0;
        const bool draw=s.n==4&&(category(hv)==4||category(hv)==5||category(hv)==8)&&category(value(hero,6))<4;
        for(int q=0;q<3;++q){
            double w=1;for(const auto& e:history)w*=likelihood(e,a,b,q);
            sum[q]+=w;sq[q]+=w*w;wins[q]+=w*result;
            if(draw)drawWins[q]+=w*result;
            drawMass[q]+=w;
        }
        ++out.samples;
    };
    if(s.n>=4){
        for(int a:deck)for(int b:deck)if(a<b){
            if(s.n==5)deal(a,b,-1,-1,-1,-1,-1);
            else for(int x:deck)if(x!=a&&x!=b)deal(a,b,x,-1,-1,-1,-1);
        }
    }else{
        const int needed=7-s.n;
        for(int i=0;i<requested;++i){
            for(int k=0;k<needed;++k){std::uniform_int_distribution<int> pick(k,int(deck.size())-1);std::swap(deck[k],deck[pick(rng)]);}
            std::array<int,5> f{{-1,-1,-1,-1,-1}};
            for(int k=0;k<5-s.n;++k)f[k]=deck[k+2];
            deal(deck[0],deck[1],f[0],f[1],f[2],f[3],f[4]);
        }
    }
    out.ready=true;
    for(int q=0;q<3;++q){
        if(sum[q]<=0||sq[q]<=0){out.ready=false;return out;}
        out.equity[q]=wins[q]/sum[q];out.effectiveSamples[q]=sum[q]*sum[q]/sq[q];
        // Equivalent winning outs under the specified HU range, including ties.
        if(s.n==4)out.winningDrawOuts[q]=44*drawWins[q]/drawMass[q];
        if(s.n<4&&out.effectiveSamples[q]<200)out.ready=false;
    }
    return out;
}
} // namespace KibitzerBoard
