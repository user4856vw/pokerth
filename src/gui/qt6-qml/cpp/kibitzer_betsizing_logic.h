#pragma once
// Stage-B opponent-response model; incremental, terminal-showdown chip EV.
// No Qt dependencies, no access to actual opponents' hidden cards.
//
// For a heads-up pot containing P chips before hero acts (including the bet
// being faced), C chips to call, and hero spending X chips to raise:
//   opponent spends X-C more when calling;
//   final pot on call = P + X + (X-C) = P+2X-C.
//   EV(raise X) = f*P + (1-f) * [ equityVsCallingRange*(P+2X-C) - X ]
// Folds recover the raise and award the pre-action pot P. This is *not*
// a GTO model, ICM valuation, or guaranteed optimal bet size.
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <holecardsrange.h>

namespace KibitzerBetSizing {

struct ResponseCounts {
    int prior = 0;
    int continueCombos = 0;
    bool subset = true;
    bool valid = false;
    double foldProbability = 0.0;
};

inline bool validKnownCards(const int hole[2], const int board[5], int boardSize,
                           std::uint64_t &mask) {
    if (boardSize != 0 && boardSize != 3 && boardSize != 4 && boardSize != 5) return false;
    mask = 0;
    for (int i=0;i<2;++i) {
        const int c=hole[i];
        if (c<0 || c>=52 || (mask & (1ULL<<c))) return false;
        mask |= 1ULL<<c;
    }
    for (int i=0;i<boardSize;++i) {
        const int c=board[i];
        if (c<0 || c>=52 || (mask & (1ULL<<c))) return false;
        mask |= 1ULL<<c;
    }
    return true;
}

// Prior nullptr = uniform any two *unseen* cards. All hand types within the
// initial range are weighted by their actual unblockable combinations.
// A response range is a DETERMINISTIC call/fold selection, not a probability
// estimated from observed behavior. Every calling hand must belong to prior.
inline ResponseCounts countResponses(const HoleCardsRange *prior,
                                     const HoleCardsRange &continuing,
                                     const int hero[2], const int board[5],
                                     int boardSize) {
    ResponseCounts out;
    std::uint64_t blockers=0;
    if (!validKnownCards(hero,board,boardSize,blockers) || continuing.empty())
        return out;
    // Verify subset without blockers. Otherwise changing the board could
    // silently "fix" a fundamentally inconsistent prior/call hypothesis.
    for (int a=0;a<51;++a) for (int b=a+1;b<52;++b) {
        if (continuing.contains(a,b) && prior && !prior->contains(a,b)) {
            out.subset = false;
            return out;
        }
    }
    for (int a=0;a<51;++a) {
        if (blockers & (1ULL<<a)) continue;
        for (int b=a+1;b<52;++b) {
            if (blockers & (1ULL<<b)) continue;
            if (prior && !prior->contains(a,b)) continue;
            ++out.prior;
            if (continuing.contains(a,b)) ++out.continueCombos;
        }
    }
    out.valid = out.prior > 0 && out.continueCombos > 0;
    if (out.valid)
        out.foldProbability = 1.0-double(out.continueCombos)/out.prior;
    return out;
}

// Useful zero-fold benchmark: ANY means "the opponent calls with the
// entire initial range", not "discard the user's prior and sample random".
inline ResponseCounts countAllResponses(const HoleCardsRange *prior,
                                        const int hero[2], const int board[5],
                                        int boardSize) {
    ResponseCounts out;
    std::uint64_t blockers=0;
    if (!validKnownCards(hero,board,boardSize,blockers)) return out;
    for (int a=0;a<51;++a) {
        if (blockers & (1ULL<<a)) continue;
        for (int b=a+1;b<52;++b) {
            if (blockers & (1ULL<<b)) continue;
            if (!prior || prior->contains(a,b)) ++out.prior;
        }
    }
    out.continueCombos=out.prior;
    out.valid=out.prior>0;
    return out; // foldProbability remains exactly zero
}

struct ActionInput {
    int pot = 0;             // pre-decision pot, including opponents' prior wagers
    int call = 0;            // amount hero must spend merely to call
    int minRaise = 0;        // PokerTH: hero's total NEW chip spend on raise
    int maxRaise = 0;        // PokerTH: cash available to hero
    int opponentCash = 0;    // excludes chips already committed in this round
    int heroSpend = 0;       // proposed NEW chip spend, not "raise to" total
    double baselineEquity = 0.0;      // prior opponent range at showdown
    double baselineStdError = 0.0;
    double continuingEquity = 0.0;    // conditional on opponent calling
    double continuingStdError = 0.0;
    double foldProbability = 0.0;
};

struct ActionResult {
    bool valid = false;
    double checkOrCallEV = 0.0;
    double foldEV = 0.0;
    double referenceEV = 0.0;
    double raiseEV = 0.0;
    double advantage = 0.0;
    double samplingMargin = 0.0;  // conservative ~95% SE bound; ignores range error
    double conditionalCallPot = 0.0;
    int opponentAdditionalCall = 0;
};

inline bool validProbability(double x) {
    return std::isfinite(x) && x >= 0.0 && x <= 1.0;
}

// The model rejects forced / opponent all-in and side-pot-like proposed
// actions. A separate caller must have enough chips to cover the *extra*
// call, and hero must retain some cash. No unlimited opponent stacks.
inline bool legal(const ActionInput &a) {
    if (a.pot <= 0 || a.call < 0 || a.minRaise <= 0 ||
        a.heroSpend <= a.call || a.heroSpend < a.minRaise ||
        a.maxRaise <= 0 || a.heroSpend >= a.maxRaise ||
        a.opponentCash <= a.heroSpend - a.call ||
        !validProbability(a.baselineEquity) ||
        !validProbability(a.continuingEquity) ||
        !validProbability(a.foldProbability) ||
        !std::isfinite(a.baselineStdError) || a.baselineStdError < 0.0 ||
        !std::isfinite(a.continuingStdError) || a.continuingStdError < 0.0)
        return false;
    return true;
}

inline ActionResult evaluate(const ActionInput &a) {
    ActionResult out;
    if (!legal(a)) return out;
    const double p=a.pot, c=a.call, x=a.heroSpend, f=a.foldProbability;
    out.valid=true;
    out.opponentAdditionalCall = a.heroSpend - a.call;
    out.conditionalCallPot = p + 2*x - c;
    // Baseline: at zero call cost, a free check; otherwise the better of
    // calling for its immediate showdown EV or folding for zero.
    out.checkOrCallEV = a.baselineEquity * (p+c) - c;
    out.referenceEV = a.call > 0 ? std::max(0.0,out.checkOrCallEV)
                                  : out.checkOrCallEV;
    out.raiseEV = f*p + (1.0-f) *
        (a.continuingEquity*out.conditionalCallPot - x);
    out.advantage = out.raiseEV - out.referenceEV;
    const double candidateSe=(1.0-f)*out.conditionalCallPot*a.continuingStdError;
    const double baselineSe=(p+c)*a.baselineStdError;
    // Conservative sum rather than independence assumption; does NOT reflect
    // uncertainty in the user-supplied ranges or the fold-response hypothesis.
    out.samplingMargin = 1.96*(candidateSe+baselineSe);
    return out;
}

inline int sizeForPotFraction(int pot, int call, int numerator, int denominator) {
    if (pot <= 0 || call < 0 || numerator <= 0 || denominator <= 0) return 0;
    const std::int64_t add=(std::int64_t(pot)*numerator + denominator/2)/denominator;
    const std::int64_t spend=std::int64_t(call)+add;
    return spend>0 && spend<=std::numeric_limits<int>::max() ? int(spend) : 0;
}

} // namespace KibitzerBetSizing
