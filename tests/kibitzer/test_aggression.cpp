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
    // Modelled strategy council: NEVER synthesize real poker-pro votes.
    {
        KibitzerCouncil::Input in;
        in.price = 20;
        in.required = 0.25;
        in.margin = 0.02;
        in.judgedEquity = 0.60;
        in.randomEquity = 0.65;
        in.manualRanges = true;
        auto c = KibitzerCouncil::assess(in);
        assert(c.lenses[0].stance == KibitzerCouncil::Stance::Call);
        assert(c.lenses[1].stance == KibitzerCouncil::Stance::Call);
        assert(!c.lenses[1].comparable); // Sensitivity is NOT a second vote.
        assert(c.consensus == KibitzerCouncil::Stance::Abstain);
        assert(c.supporting == 1);
        assert(!c.sensitiveToRanges);

        in.randomEquity = 0.05; // Both hypotheses no longer corroborate.
        c = KibitzerCouncil::assess(in);
        assert(c.sensitiveToRanges);
        assert(c.lenses[1].stance == KibitzerCouncil::Stance::Abstain);
        assert(c.consensus == KibitzerCouncil::Stance::Abstain);

        in.randomEquity = 0.65;
        in.betEvReady = true;
        in.betEvRaiseEdge = true; // Bet EV conflicts with passive CALL.
        in.aggressionCandidate = true;
        c = KibitzerCouncil::assess(in);
        assert(c.lenses[2].stance == KibitzerCouncil::Stance::ExploreRaise);
        assert(!c.lenses[2].comparable); // A candidate is not an actionable vote.
        assert(c.lenses[3].stance == KibitzerCouncil::Stance::Raise);
        assert(c.consensus == KibitzerCouncil::Stance::Abstain);
        assert(c.conflicting == 0); // CALL viable and RAISE better can BOTH be true.

        in.betEvRaiseEdge = false;
        in.betEvPassiveEdge = true;
        c = KibitzerCouncil::assess(in);
        assert(c.lenses[3].stance == KibitzerCouncil::Stance::Passive);
        assert(c.consensus == KibitzerCouncil::Stance::Call);
        assert(c.supporting == 2);

        in.manualRanges = false;
        in.betEvReady = false;
        in.betEvPassiveEdge = false;
        c = KibitzerCouncil::assess(in);
        assert(c.lenses[1].stance == KibitzerCouncil::Stance::Abstain);
        assert(c.consensus == KibitzerCouncil::Stance::Abstain);
    }
    {
        KibitzerCouncil::Input in;
        in.price = 0;
        in.randomEquity = 0.40;
        in.judgedEquity = 0.40;
        in.betEvReady = true;
        in.betEvPassiveEdge = true;
        const auto c = KibitzerCouncil::assess(in);
        assert(c.consensus == KibitzerCouncil::Stance::Check);
        assert(c.supporting == 2);
    }
    std::cout << "Stage-A aggression tests passed\n";
}
