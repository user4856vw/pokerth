#include "kibitzer_aggression_logic.h"
#include <cassert>
#include <iostream>

using namespace KibitzerAggression;
namespace {
int code(int rank, int suit) { return suit * 13 + rank - 2; }
Input make() {
    Input in;
    in.hero[0] = code(14,2); in.hero[1] = code(14,1); // A spade / A heart
    in.opponents = 1;
    in.minRaise = 20; in.maxRaise = 200;
    in.equity = 0.88; in.standardError = 0.006; in.samples = 2500;
    return in;
}
}

int main() {
    {
        Input in = make();
        assert(assess(in).signal == Signal::PremiumPreflop);
        in.manualRanges = true;
        in.equity = 0.50; // AA versus AA / hypothetical perfectly tight range
        assert(assess(in).signal == Signal::None);
        in.equity = 0.65;
        assert(assess(in).signal == Signal::PremiumPreflop);
    }
    {
        Input in = make();
        in.boardSize = 3;
        in.board[0] = code(2,0); in.board[1] = code(7,1);
        in.board[2] = code(10,3);
        assert(assess(in).signal == Signal::ValueCandidate);
        in.equity = 0.79;
        assert(assess(in).signal == Signal::None);
        in.equity = 0.9;
        in.maxRaise = 20; // only forced all-in
        assert(assess(in).signal == Signal::None);
        in.maxRaise = 200;
        in.minRaise = 180; // practically an all-in
        assert(assess(in).signal == Signal::None);
        in.minRaise = 20;
        in.allInPresent = true;
        assert(assess(in).signal == Signal::None);
        in.allInPresent = false;
        in.samples = 300;
        assert(assess(in).signal == Signal::None);
    }
    {
        Input in = make();
        in.hero[0] = code(12,2); in.hero[1] = code(11,2);
        in.board[0] = code(2,2); in.board[1] = code(6,2);
        in.board[2] = code(9,0);
        in.boardSize = 3;
        in.equity = 0.43;
        assert(hasFlushDraw(in));
        assert(assess(in).signal == Signal::SemiBluffCandidate);
        in.price = 40; // a re-raise bluff requires the missing fold-response model
        assert(assess(in).signal == Signal::None);
        in.price = 0;
        in.opponents = 4;
        assert(assess(in).signal == Signal::None);
        in.opponents = 1;
        in.board[2] = code(9,2); // made flush, not a draw
        assert(!hasFlushDraw(in));
        assert(assess(in).signal == Signal::None);
    }
    {
        Input in = make();
        in.hero[0] = code(6,0); in.hero[1] = code(7,1);
        in.board[0] = code(8,2); in.board[1] = code(9,3);
        in.board[2] = code(14,0);
        in.boardSize = 3; in.equity = 0.35;
        assert(hasOpenStraightDraw(in));
        assert(assess(in).signal == Signal::SemiBluffCandidate);
        in.board[2] = code(10,0); // 6..10 made straight
        assert(!hasOpenStraightDraw(in));
        assert(assess(in).signal == Signal::None);
    }
    {
        Input in = make();
        in.hero[0] = code(14,0); in.hero[1] = code(13,1);
        in.boardSize = 0; in.equity = 0.61;
        assert(!premiumPreflop(in.hero)); // offsuit AK is not our narrow premium flag
        assert(assess(in).signal == Signal::None);
        in.hero[1] = code(13,0); // suited AKs
        in.hero[0] = code(14,0);
        assert(premiumPreflop(in.hero));
        assert(assess(in).signal == Signal::PremiumPreflop);
    }
    std::cout << "Stage-A aggression tests passed\n";
}
