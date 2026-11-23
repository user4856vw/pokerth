#pragma once
// Historical WSOP hand *retrieval*, not a solver, expert vote or optimal action.
// Pure standard C++ for independent regression testing.
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace KibitzerProHistory {

struct Situation {
    std::string street;
    std::string position;
    int playersInHand = 0;
    double stackBB = 0.0; // chips behind before the decision, current BB
    double potBB = 0.0;   // pot before the decision, current BB
    double toCallBB = 0.0;
    std::array<int,2> ranks{{0,0}};   // 2..14
    std::array<int,2> suits{{-1,-1}};
    std::array<int,5> boardRanks{{0,0,0,0,0}};
    std::array<int,5> boardSuits{{-1,-1,-1,-1,-1}};
    int boardCount = 0;
};

inline int rank(char c) {
    const std::string all="23456789TJQKA";
    const auto index=all.find(c);
    return index==std::string::npos ? 0 : int(index)+2;
}
inline int suit(char c) {
    const auto index=std::string("cdhs").find(c);
    return index==std::string::npos ? -1 : int(index);
}
inline bool setCards(Situation &s,const std::string &hole,const std::string &board) {
    if (hole.size()!=4 || board.size()!=0 && board.size()!=6 &&
        board.size()!=8 && board.size()!=10) return false;
    s.boardCount=int(board.size())/2;
    for (int i=0;i<2;++i) {
        s.ranks[i]=rank(hole[2*i]);s.suits[i]=suit(hole[2*i+1]);
        if (!s.ranks[i] || s.suits[i]<0) return false;
    }
    for (int i=0;i<s.boardCount;++i) {
        s.boardRanks[i]=rank(board[2*i]);s.boardSuits[i]=suit(board[2*i+1]);
        if (!s.boardRanks[i] || s.boardSuits[i]<0) return false;
    }
    return true;
}
inline bool fromPokerth(Situation &s,const int hole[2],
                        const int board[5],int boardSize) {
    if (boardSize!=0 && boardSize!=3 && boardSize!=4 && boardSize!=5)
        return false;
    s.boardCount=boardSize;
    for (int i=0;i<2;++i) {
        if (hole[i]<0 || hole[i]>=52) return false;
        s.ranks[i]=hole[i]%13+2;
        s.suits[i]=hole[i]/13;
    }
    for (int i=0;i<boardSize;++i) {
        if (board[i]<0 || board[i]>=52) return false;
        s.boardRanks[i]=board[i]%13+2;
        s.boardSuits[i]=board[i]/13;
    }
    return true;
}
inline bool isValid(const Situation &s) {
    if (s.ranks[0]<2 || s.ranks[0]>14 || s.ranks[1]<2 || s.ranks[1]>14 ||
        s.suits[0]<0 || s.suits[0]>3 || s.suits[1]<0 || s.suits[1]>3 ||
        s.playersInHand<2 || s.playersInHand>10 || s.stackBB<0.0 ||
        s.potBB<=0 || s.toCallBB<0 || !std::isfinite(s.stackBB) ||
        !std::isfinite(s.potBB) || !std::isfinite(s.toCallBB)) return false;
    const int expected=s.street=="preflop"?0:s.street=="flop"?3:
        s.street=="turn"?4:s.street=="river"?5:-1;
    if (expected!=s.boardCount || expected<0) return false;
    return true;
}
inline bool isPair(const Situation &s) {return s.ranks[0]==s.ranks[1];}
inline bool isSuited(const Situation &s) {return s.suits[0]==s.suits[1];}
inline int highRank(const Situation &s) {return std::max(s.ranks[0],s.ranks[1]);}
inline int lowRank(const Situation &s) {return std::min(s.ranks[0],s.ranks[1]);}
inline int rankHitCount(const Situation &s) {
    int hit=0;
    for(int k=0;k<2;++k) for(int j=0;j<s.boardCount;++j)
        if(s.ranks[k]==s.boardRanks[j]) {++hit;break;}
    return hit;
}
inline bool boardPaired(const Situation &s) {
    for(int j=0;j<s.boardCount;++j)for(int k=j+1;k<s.boardCount;++k)
        if(s.boardRanks[j]==s.boardRanks[k])return true;
    return false;
}
inline int boardSuitMaximum(const Situation &s) {
    int count[4]={0,0,0,0};
    for(int j=0;j<s.boardCount;++j)++count[s.boardSuits[j]];
    return *std::max_element(count,count+4);
}
inline int heroSuitMaximum(const Situation &s) {
    int count[4]={0,0,0,0};
    for(int j=0;j<s.boardCount;++j)++count[s.boardSuits[j]];
    for(int j=0;j<2;++j)++count[s.suits[j]];
    return *std::max_element(count,count+4);
}
inline bool sameOpponentScale(int x,int y) {
    return (x==2)==(y==2); // heads-up versus multiway is never interchangeable
}

// Hard gates avoid misleading analogies. Score describes only the observed
// structural features, not statistical power, truth of the play, or EV.
struct Match {
    bool eligible=false;
    bool close=false;
    double score=0.0; // 0..100 for surviving hard constraints
};
inline Match compare(const Situation &now,const Situation &past) {
    Match out;
    if (!isValid(now) || !isValid(past) ||
        now.street!=past.street ||
        !sameOpponentScale(now.playersInHand,past.playersInHand) ||
        (now.toCallBB>0.001)!=(past.toCallBB>0.001) ||
        isPair(now)!=isPair(past)) return out;

    // Blind/BTN role mismatch changes opening ranges materially. CO and OTHER
    // are treated as nearby but not identical, since PokerTH lacks a robust
    // standardized seat taxonomy for every table configuration.
    auto explicitRole=[](const std::string &s) {
        return s=="BTN"||s=="BTN/SB"||s=="BB"||s=="SB";
    };
    if ((explicitRole(now.position)||explicitRole(past.position)) &&
        now.position!=past.position) return out;
    if (!isPair(now) && isSuited(now)!=isSuited(past)) return out;

    const int highGap=std::abs(highRank(now)-highRank(past));
    const int lowGap=std::abs(lowRank(now)-lowRank(past));
    if (highGap>3 || lowGap>3) return out;

    if (now.stackBB>0 && past.stackBB>0) {
        const double ratio=now.stackBB/past.stackBB;
        if (ratio<0.4 || ratio>2.5) return out;
    } else if (now.stackBB!=past.stackBB) return out;

    if (now.boardCount) {
        if (boardPaired(now)!=boardPaired(past) ||
            boardSuitMaximum(now)!=boardSuitMaximum(past) ||
            rankHitCount(now)!=rankHitCount(past) ||
            (heroSuitMaximum(now)>=4)!=(heroSuitMaximum(past)>=4) ||
            (heroSuitMaximum(now)>=5)!=(heroSuitMaximum(past)>=5))
            return out;
    }
    // Enforce broadly comparable pressure. 6 BB to call into 10 BB is very
    // different from 0.2 BB into 10 BB, even with equal hole cards.
    const double nowRatio=now.toCallBB/now.potBB;
    const double pastRatio=past.toCallBB/past.potBB;
    if (std::abs(nowRatio-pastRatio)>0.25) return out;

    double score=100.0;
    score-=3.0*highGap+2.5*lowGap;
    if (now.position!=past.position)score-=8.0;
    score-=std::min(13.0,9.0*std::abs(std::log((now.stackBB+1.0)/(past.stackBB+1.0))));
    score-=std::min(8.0,5.0*std::abs(std::log((now.potBB+1.0)/(past.potBB+1.0))));
    score-=std::min(13.0,30.0*std::abs(nowRatio-pastRatio));
    if (now.boardCount) {
        score-=1.5*std::abs(now.boardRanks[0]-past.boardRanks[0]);
    }
    out.eligible=true;
    out.score=std::max(0.0,std::min(100.0,score));
    out.close=out.score>=82.0; // only structural fit, never strategic approval
    return out;
}

} // namespace KibitzerProHistory
