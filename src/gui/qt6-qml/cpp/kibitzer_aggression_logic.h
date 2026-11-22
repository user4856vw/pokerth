#pragma once
// Educational Stage-A aggression signals. Deliberately NOT a bet-sizing solver.
// Uses only the hero's cards, public board, legal action limits, and estimated equity.
#include <algorithm>
#include <cmath>

namespace KibitzerAggression {

enum class Signal { None, PremiumPreflop, ValueCandidate, SemiBluffCandidate };

struct Input {
    int hero[2] = {-1, -1};      // PokerTH: rank = card % 13 (2..A), suit = card / 13
    int board[5] = {-1, -1, -1, -1, -1};
    int boardSize = 0;
    int opponents = 0;
    int price = 0;               // chips to call
    int minRaise = 0;            // engine: amount of the smallest bet/raise
    int maxRaise = 0;            // engine: amount of the largest bet/raise
    bool allInPresent = false;   // side-pot complications are out of scope
    bool manualRanges = false;
    double equity = 0.0;        // conditional showdown pot share, not P(a call)
    double standardError = 0.0;
    int samples = 0;
};

struct Assessment {
    Signal signal = Signal::None;
    double lowerEquity = 0.0;
    bool premium = false;
    bool flushDraw = false;
    bool openStraightDraw = false;
};

inline bool validCard(int c) { return c >= 0 && c < 52; }

// Strong starting hands can justify opening aggression even at a full table,
// where showdown equity against ALL opponents is inevitably much less than 80%.
// Position, stack size, 3-bets and caller selection still matter.
inline bool premiumPreflop(const int hero[2]) {
    if (!validCard(hero[0]) || !validCard(hero[1])) return false;
    const int a = hero[0] % 13, b = hero[1] % 13;
    const bool pair = a == b;
    const bool aceKingSuited = ((a == 12 && b == 11) || (a == 11 && b == 12))
                              && hero[0] / 13 == hero[1] / 13;
    return (pair && a >= 10) || aceKingSuited; // QQ+, AKs only; intentionally narrow
}

inline bool hasFlushDraw(const Input &in) {
    if (in.boardSize != 3 && in.boardSize != 4) return false;
    int suits[4] = {};
    for (int c : in.hero) if (validCard(c)) ++suits[c / 13];
    for (int i = 0; i < in.boardSize; ++i)
        if (validCard(in.board[i])) ++suits[in.board[i] / 13];
    // At least one hero card must contribute; four on the board isn't a
    // hero draw. A made flush (five cards) is not a drawing hand.
    for (int suit = 0; suit < 4; ++suit)
        if (suits[suit] == 4 &&
            (in.hero[0] / 13 == suit || in.hero[1] / 13 == suit)) return true;
    return false;
}

inline bool hasOpenStraightDraw(const Input &in) {
    if (in.boardSize != 3 && in.boardSize != 4) return false;
    bool ranks[15] = {};
    for (int c : in.hero) if (validCard(c)) ranks[c % 13 + 2] = true;
    for (int i = 0; i < in.boardSize; ++i)
        if (validCard(in.board[i])) ranks[in.board[i] % 13 + 2] = true;
    // A four-card run with BOTH possible ends. Do not count A-2-3-4 or
    // J-Q-K-A as open-ended (one possible completing rank only).
    for (int low = 3; low <= 10; ++low) {
        if (!ranks[low] || !ranks[low + 1] ||
            !ranks[low + 2] || !ranks[low + 3]) continue;
        if (!ranks[low - 1] && !ranks[low + 4]) {
            for (int card : in.hero)
                if (validCard(card) && card % 13 + 2 >= low
                    && card % 13 + 2 <= low + 3) return true;
        }
    }
    return false;
}

inline Assessment assess(const Input &in) {
    Assessment out;
    if (in.opponents < 1 || in.opponents > 9 || in.price < 0 ||
        in.samples < 1000 || !std::isfinite(in.equity) ||
        !std::isfinite(in.standardError) || in.equity < 0 || in.equity > 1 ||
        in.standardError < 0 || in.boardSize < 0 || in.boardSize > 5 ||
        !validCard(in.hero[0]) || !validCard(in.hero[1]) ||
        in.hero[0] == in.hero[1] || in.allInPresent) return out;
    // A minimum bet equal/nearly equal to the entire available stack is
    // effectively an all-in. Stage A should not advocate that implicitly.
    if (in.minRaise <= 0 || in.maxRaise <= in.minRaise ||
        in.maxRaise <= in.price ||
        5LL * in.minRaise >= 4LL * in.maxRaise) return out;

    const double monteCarloMargin = 1.96 * in.standardError;
    // A heuristic caution haircut for an uninformative uniform-random model.
    const double caution = in.manualRanges ? monteCarloMargin
                                           : std::max(0.12, monteCarloMargin);
    out.lowerEquity = std::max(0.0, in.equity - caution);
    out.premium = in.boardSize == 0 && premiumPreflop(in.hero);

    // Do not suggest even premium raises against a manually entered
    // range where the estimate says our hand is very poorly placed.
    if (out.premium && (!in.manualRanges ||
                        (in.equity >= 0.58 && out.lowerEquity >= 0.53))) {
        out.signal = Signal::PremiumPreflop;
        return out;
    }

    // Equity is the chance of sharing the pot at showdown against a range,
    // NOT the equity against the SUBSET of hands which will call our bet.
    // Thus this is merely a candidate for further consideration.
    if (in.equity >= 0.82 && out.lowerEquity >= 0.70) {
        out.signal = Signal::ValueCandidate;
        return out;
    }

    // An unmade flush or open-ended straight draw may provide a semi-bluff,
    // but without fold probabilities it is NEVER an automatic bet instruction.
    out.flushDraw = hasFlushDraw(in);
    out.openStraightDraw = hasOpenStraightDraw(in);
    if (in.boardSize >= 3 && in.boardSize <= 4 && in.opponents <= 2 &&
        (out.flushDraw || out.openStraightDraw) &&
        in.equity >= 0.22 && in.equity < 0.70) {
        out.signal = Signal::SemiBluffCandidate;
    }
    return out;
}

} // namespace KibitzerAggression
