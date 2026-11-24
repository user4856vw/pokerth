// Conservative, bounded river bluff-catching for local autonomous trials.
// Does NOT infer an opponent's bluff frequency. The caller may risk a SMALL
// amount to call a re-raise when holding some showdown value; large bets
// without an action-conditioned range must be declined.
//
// Card encoding: 0..51, rank = code % 13 + 2, suit = code / 13.
#pragma once
#include <array>
#include <cstdint>

namespace KibitzerRiverDefense {
inline int rank(int card) { return card % 13 + 2; }
inline bool valid(int card) { return card >= 0 && card < 52; }

// Is there evidence that our hole cards improve a paired board or can
// participate in a straight/flush? A pair solely on the board with two
// irrelevant hole cards is NOT enough for this conservative bluff catch.
inline bool hasShowdownHand(int h0, int h1, const std::array<int,5>& board) {
    if (!valid(h0) || !valid(h1) || h0 == h1) return false;
    std::array<bool,52> seen{};
    seen[h0]=true;
    seen[h1]=true;
    std::array<int,4> suitCount{};
    std::array<bool,15> ranks{};
    const std::array<int,7> all{h0,h1,board[0],board[1],board[2],
                                 board[3],board[4]};
    for (int card:all) {
        if (!valid(card) || (card!=h0 && card!=h1 && seen[card]))
            return false;
        seen[card]=true;
        ++suitCount[card/13];
        ranks[rank(card)]=true;
    }
    if (rank(h0)==rank(h1)) return true;
    for (int card:board)
        if (rank(card)==rank(h0) || rank(card)==rank(h1)) return true;
    for (int suit=0;suit<4;++suit)
        if (suitCount[suit]>=5 && (h0/13==suit || h1/13==suit))
            return true;
    ranks[1]=ranks[14]; // wheel straight
    for (int top=5;top<=14;++top) {
        bool straight=true;
        for (int r=top-4;r<=top;++r)
            if (!ranks[r]) straight=false;
        if (straight) {
            const int r0=rank(h0),r1=rank(h1);
            for (int r=top-4;r<=top;++r)
                if (r==r0 || r==r1 || (r==1 && (r0==14 || r1==14)))
                    return true;
        }
    }
    return false;
}

struct Input {
    int price=0;            // cost to call, not a total investment
    int pot=0;              // includes the opponent's bet, before our call
    int remainingStack=0;   // cash before this decision
    int hero0=-1,hero1=-1;
    std::array<int,5> board{{-1,-1,-1,-1,-1}};
};

inline bool allowCheapCall(const Input &in) {
    if (in.price<=0 || in.pot<=0 || in.remainingStack<=0 ||
        in.price>in.remainingStack) return false;
    // Both limits matter. A call must be cheap RELATIVE TO THE POT and
    // our remaining stack; a massive pot does not excuse jeopardizing the
    // entire tournament. Multiplication is widened to prevent overflow.
    constexpr int kMaxPctPot=12;   // <=12% of already committed pot
    constexpr int kMaxPctStack=6;  // <=6% of remaining stack
    if (int64_t(in.price)*100 > int64_t(in.pot)*kMaxPctPot ||
        int64_t(in.price)*100 > int64_t(in.remainingStack)*kMaxPctStack)
        return false;
    return hasShowdownHand(in.hero0,in.hero1,in.board);
}
} // namespace KibitzerRiverDefense
