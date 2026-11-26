// Board texture and one-card draw diagnostics. No hidden cards or runout.
// The shared engine evaluator handles the same 5/6/7-card ordering.
#pragma once
#include "handevaluator.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace KibitzerBoard {
using Board=std::array<int,5>;
inline bool card(int x){return x>=0 && x<52;}
inline bool valid(int a,int b,const Board& board,int n) {
    if(n<0 || n>5 || !card(a)||!card(b)||a==b) return false;
    uint64_t seen=(1ULL<<a)|(1ULL<<b);
    for(int i=0;i<n;++i){
        if(!card(board[i]) || (seen&(1ULL<<board[i]))) return false;
        seen|=1ULL<<board[i];
    }
    return true;
}
inline uint64_t mask(int a,int b,const Board& board,int n) {
    uint64_t m=(1ULL<<a)|(1ULL<<b);
    for(int i=0;i<n;++i)m|=1ULL<<board[i];
    return m;
}
inline unsigned value(int a,int b,const Board& board,int n) {
    int h[7]={a,b};
    for(int i=0;i<n;++i)h[2+i]=board[i];
    return HandEvaluator::value(h,n+2);
}
inline int category(unsigned v){return int(v>>20);}
struct Texture {
    int pairs=0,maxRankCount=0,maxSuitCount=0,boardCategory=0;
    bool straightPossible=false;
};
inline Texture texture(const Board& b,int n) {
    Texture t;
    int r[13]={},s[4]={};
    unsigned bits=0;
    for(int i=0;i<n;++i){++r[b[i]%13];++s[b[i]/13];bits|=1u<<(b[i]%13);}
    for(int x:r){t.pairs+=x>=2;t.maxRankCount=std::max(t.maxRankCount,x);}
    for(int x:s)t.maxSuitCount=std::max(t.maxSuitCount,x);
    for(int i=0;i<=9;++i){
        unsigned window=i==0?((1u<<12)|15u):(31u<<(i-1));
        int c=0;for(unsigned q=bits&window;q;q&=q-1)++c;
        if(c>=3)t.straightPossible=true;
    }
    t.boardCategory=n==5 ? category(HandEvaluator::value(b.data(),5)):
        t.maxRankCount==4?7:t.maxRankCount==3?(t.pairs>=2?6:3):t.pairs>=2?2:t.pairs==1?1:0;
    return t;
}
// Does a private card improve the category, rather than simply supply
// a kicker to a community pair? A stronger straight/flush on a made
// five-card board also counts. Kicker-only hands can still win bluffs.
inline bool privateMade(int,int,const Board& board,int n,unsigned v) {
    const auto t=texture(board,n);
    const int c=category(v);
    return c>t.boardCategory ||
        (n==5 && c>=4 && v>HandEvaluator::value(board.data(),5));
}
inline bool oneCardDraw(int a,int b,const Board& board,int n) {
    if(n<3||n>=5) return false;
    int suits[4]={};unsigned bits=0;
    for(int i=0;i<n;++i){++suits[board[i]/13];bits|=1u<<(board[i]%13);}
    ++suits[a/13];++suits[b/13];bits|=(1u<<(a%13))|(1u<<(b%13));
    for(int s:suits) if(s==4)return true;
    for(int i=0;i<=9;++i){
        unsigned window=i==0?((1u<<12)|15u):(31u<<(i-1));
        int count=0;for(unsigned q=bits&window;q;q&=q-1)++count;
        if(count==4)return true;
    }
    return false;
}
struct Analysis {
    bool valid=false,privateHand=false;
    int madeCategory=0,nominalDrawOuts=0,unbeatableDrawOuts=0;
    int boardPairs=0,maxBoardRank=0,maxBoardSuit=0;
    double currentAhead=0,currentTie=0;
    // Per next-card diagnostics against all legal opposing holdings.
    // "unbeatable" means cannot be beaten on THAT next street, not
    // guaranteed sole win or guaranteed by the river from the flop.
    std::array<bool,52> drawOut{};
    std::array<double,52> drawOutEquity{};
};
inline Analysis analyze(int a,int b,const Board& board,int n) {
    Analysis out;
    if(n<3||!valid(a,b,board,n))return out;
    out.valid=true;
    const auto t=texture(board,n);
    out.boardPairs=t.pairs;out.maxBoardRank=t.maxRankCount;out.maxBoardSuit=t.maxSuitCount;
    const unsigned own=value(a,b,board,n);
    out.madeCategory=category(own);
    out.privateHand=privateMade(a,b,board,n,own);
    const uint64_t known=mask(a,b,board,n);
    int total=0,ahead=0,tie=0;
    for(int x=0;x<51;++x)if(!(known&(1ULL<<x)))
        for(int y=x+1;y<52;++y)if(!(known&(1ULL<<y))){
            const auto v=value(x,y,board,n);
            ++total;ahead+=own>v;tie+=own==v;
        }
    if(total){out.currentAhead=double(ahead)/total;out.currentTie=double(tie)/total;}
    if(n>=5||out.madeCategory>=4)return out;
    for(int c=0;c<52;++c)if(!(known&(1ULL<<c))){
        Board next=board;next[n]=c;
        const unsigned improved=value(a,b,next,n+1);
        // Straight/flush or better involving our hand, not a board-only
        // straight/flush, and not counting mere high-card improvements.
        if(category(improved)<4 || !privateMade(a,b,next,n+1,improved))continue;
        // Count only a new straight or flush, not a spurious board trip
        // or a full house from a different kind of made-hand improvement.
        if(category(improved)!=4 && category(improved)!=5 && category(improved)!=8)continue;
        out.drawOut[c]=true;++out.nominalDrawOuts;
        const uint64_t used=known|(1ULL<<c);
        int wins=0,ties=0,trials=0,losses=0;
        for(int x=0;x<51;++x)if(!(used&(1ULL<<x)))
            for(int y=x+1;y<52;++y)if(!(used&(1ULL<<y))){
                const auto v=value(x,y,next,n+1);
                ++trials;wins+=improved>v;ties+=improved==v;losses+=improved<v;
            }
        if(trials){
            out.drawOutEquity[c]=(wins+0.5*ties)/trials;
            if(losses==0)++out.unbeatableDrawOuts;
        }
    }
    return out;
}
} // namespace KibitzerBoard
