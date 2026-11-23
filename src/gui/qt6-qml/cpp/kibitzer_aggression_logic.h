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
    // Facing a bet, a semi-bluff raise requires a credible fold-response
    // model. For Stage A, only flag opportunities where checking is free.
    if (in.boardSize >= 3 && in.boardSize <= 4 && in.opponents <= 2 &&
        in.price == 0 && (out.flushDraw || out.openStraightDraw) &&
        in.equity >= 0.22 && in.equity < 0.70) {
        out.signal = Signal::SemiBluffCandidate;
    }
    return out;
}

} // namespace KibitzerAggression


// A transparent cross-check of four EDUCATIONAL STRATEGY LENSES.
// These are deterministic models built from the visible game state, NOT
// statements, votes or endorsements by any real professional poker player.
// The range-robustness lens deliberately shares inputs with the pot-odds
// lens: its alignment is a sensitivity check, not independent confirmation.
namespace KibitzerCouncil {

enum class Stance { Abstain, Fold, Check, Call, Raise, Passive, ExploreRaise };

struct Input {
    int price = 0;
    double randomEquity = 0.0;
    double judgedEquity = 0.0;
    double required = 0.0;
    double margin = 0.0;
    bool manualRanges = false;
    bool aggressionCandidate = false;
    bool betEvReady = false;
    bool betEvRaiseEdge = false;
    bool betEvPassiveEdge = false;
};

struct Lens {
    const char *label = "";
    Stance stance = Stance::Abstain;
    const char *reason = "";
    bool comparable = false;  // actual calculation; not a vote of a person
};

struct Result {
    Lens lenses[4];
    Stance consensus = Stance::Abstain;
    int comparable = 0;
    int supporting = 0;
    int conflicting = 0;
    bool sensitiveToRanges = false;
};

inline Stance potOdds(double equity, double threshold, double margin, int price) {
    if (price == 0) return Stance::Check; // Only a safe fallback, not optimality.
    if (!std::isfinite(equity) || !std::isfinite(threshold) ||
        !std::isfinite(margin) || margin < 0.0 || equity < 0.0 || equity > 1.0)
        return Stance::Abstain;
    if (equity > threshold + margin) return Stance::Call;
    if (equity < threshold - margin) return Stance::Fold;
    return Stance::Abstain;
}

inline Result assess(const Input &in) {
    Result r;
    if (in.price < 0 || !std::isfinite(in.randomEquity) ||
        !std::isfinite(in.judgedEquity) || !std::isfinite(in.required) ||
        !std::isfinite(in.margin) || in.margin < 0.0 ||
        in.randomEquity < 0.0 || in.randomEquity > 1.0 ||
        in.judgedEquity < 0.0 || in.judgedEquity > 1.0 ||
        in.required < 0.0 || in.required > 1.0) return r;

    // Important: positive CALL EV versus FOLD does NOT mean CALL is better
    // than RAISE. This first lens makes only that narrow comparison.
    const Stance priced = potOdds(in.judgedEquity, in.required, in.margin, in.price);
    r.lenses[0] = {"Pot-odds discipline", priced,
        in.price == 0 ? "A check is free; no assertion that checking beats betting."
        : "Compare CALL versus FOLD using conditioned showdown equity and its displayed margin. No claim versus raising.",
        priced != Stance::Abstain};

    // A sensitivity check on the SAME comparison is not an independent vote.
    const Stance random = potOdds(in.randomEquity, in.required,
                                  std::max(0.12, in.margin), in.price);
    Stance robust = Stance::Abstain;
    if (in.manualRanges && in.price > 0 &&
        priced != Stance::Abstain && random == priced)
        robust = priced;
    r.sensitiveToRanges = in.manualRanges && in.price > 0 &&
        priced != Stance::Abstain && random != priced;
    r.lenses[1] = {"Assumption stress test", robust,
        !in.manualRanges ? "Requires your assigned opponent ranges; random-only data cannot test sensitivity."
        : r.sensitiveToRanges ? "Random and selected-range hypotheses do not give the same clear CALL-vs-FOLD result."
        : robust != Stance::Abstain ? "Both hypotheses pass their respective caution margins for the SAME CALL-vs-FOLD result."
        : "No result survives both range-hypothesis margins.",
        false};

    r.lenses[2] = {"Value and pressure scout",
        in.aggressionCandidate ? Stance::ExploreRaise : Stance::Abstain,
        in.aggressionCandidate
            ? "Stage A flags a possible premium/value/draw raise to STUDY. No positive raise EV is established."
            : "No Stage-A pressure flag; this is not evidence against betting.",
        false};

    // This is the ONLY lens comparing a specified raise with the passive
    // alternative. Its samples and assumptions are not expert votes.
    const Stance betting = !in.betEvReady ? Stance::Abstain
        : in.betEvRaiseEdge ? Stance::Raise
        : in.betEvPassiveEdge ? Stance::Passive
        : Stance::Abstain;
    r.lenses[3] = {"Response-aware chip EV", betting,
        !in.betEvReady ? "Requires a heads-up continuing-range model; unavailable for this decision."
        : in.betEvRaiseEdge ? "A tested bet size beats the best passive action beyond the sampling margin, conditional on opponent responses."
        : in.betEvPassiveEdge ? "Tested raises lose to the best passive action; pot odds separately choose CALL/FOLD or free CHECK."
        : "Best tested raise is within Monte Carlo sampling error of the passive option.",
        betting != Stance::Abstain};

    r.comparable = int(r.lenses[0].comparable) + int(r.lenses[3].comparable);
    // These checks address complementary comparisons. Only a significant
    // PASSIVE result plus a resolved FOLD/CALL (or free CHECK) can compose
    // into an action among the tested alternatives. A RAISE edge is a
    // single conditional EV result, NOT a multi-expert consensus.
    if (betting == Stance::Passive && priced != Stance::Abstain) {
        r.consensus = priced;
        r.supporting = 2;
    } else if (r.comparable > 0) {
        r.supporting = 1;
    }
    return r;
}

} // namespace KibitzerCouncil
