#pragma once
// Standalone, deterministic PokerTH training statistics. No Qt dependencies.
// These metrics describe recorded choices, NOT solver-derived decision quality.
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace KibitzerTrainingStats {
struct Decision {
    std::string phase;
    std::string action;
    std::string rating;
    int price = 0;
    bool aggressive = false;
    std::string position; // BTN/SB/BB/Other, assigned when a decision is recorded
};
struct Hand {
    int session = 0;
    std::vector<Decision> decisions;
};
struct Summary {
    int hands = 0;
    int decisions = 0;
    int folds = 0;
    int checks = 0;
    int calls = 0;
    int raises = 0;
    int allIns = 0;
    int freeCheckOpportunities = 0;
    int clearMistakes = 0;
    int pricedReviewed = 0;   // PAID fold/call decisions actually compared
    int reviewWarnings = 0;   // only within pricedReviewed
    int ungraded = 0;
    int preflopObservedHands = 0; // a preflop human decision was actually recorded
    int vpipHands = 0;           // voluntary preflop investments, blinds excluded
    int pfrHands = 0;            // voluntary preflop raises
    int postflopAggressive = 0;
    int postflopPassive = 0;    // check or call (not folds)
};
inline bool isPreflop(const Decision &d) { return d.phase == "Preflop"; }
inline void addHand(Summary &s, const Hand &hand) {
    if (hand.decisions.empty()) return;
    ++s.hands;
    bool sawPreflop = false, voluntary = false, raised = false;
    for (const auto &d : hand.decisions) {
        ++s.decisions;
        const bool fold = d.action == "FOLD";
        const bool checkCall = d.action == "CHECK/CALL";
        const bool allIn = d.action == "ALL-IN";
        const bool raise = d.action == "BET/RAISE";
        if (fold) ++s.folds;
        if (checkCall && d.price == 0) ++s.checks;
        if (checkCall && d.price > 0) ++s.calls;
        if (raise) ++s.raises;
        if (allIn) ++s.allIns;
        if (d.price == 0) ++s.freeCheckOpportunities;
        if (d.rating == "CLEAR MISTAKE") ++s.clearMistakes;
        if (d.rating == "NOT GRADED") ++s.ungraded;
        // The random-range review flag has a valid denominator only for
        // priced fold/call decisions that were actually scored.
        if (d.price > 0 && (fold || checkCall) &&
            (d.rating == "REVIEW" || d.rating == "NO FLAG")) {
            ++s.pricedReviewed;
            if (d.rating == "REVIEW") ++s.reviewWarnings;
        }
        if (isPreflop(d)) {
            sawPreflop = true;
            if (raise || allIn || (checkCall && d.price > 0)) voluntary = true;
            if (raise || (allIn && d.aggressive)) raised = true;
        } else if (d.phase == "Flop" || d.phase == "Turn" || d.phase == "River") {
            if (raise || (allIn && d.aggressive)) ++s.postflopAggressive;
            else if (checkCall) ++s.postflopPassive;
            // Non-aggressive all-in calls are passive.
            else if (allIn) ++s.postflopPassive;
        }
    }
    if (sawPreflop) {
        ++s.preflopObservedHands;
        if (voluntary) ++s.vpipHands;
        if (raised) ++s.pfrHands;
    }
}
inline Summary summarize(const std::vector<Hand>& hands, std::size_t begin,
                         std::size_t end) {
    Summary out;
    begin = std::min(begin, hands.size());
    end = std::min(end, hands.size());
    for (std::size_t i = begin; i < end; ++i) addHand(out, hands[i]);
    return out;
}
inline double percent(int numerator, int denominator) {
    return denominator > 0 ? 100.0 * numerator / denominator : 0.0;
}
inline std::vector<Hand> preflopPositionOnly(const std::vector<Hand>& hands,
                                             const std::string &position) {
    std::vector<Hand> out;
    for (const Hand &h : hands) {
        Hand copy;
        copy.session=h.session;
        for (const Decision &d : h.decisions)
            if (d.phase == "Preflop" && d.position == position) copy.decisions.push_back(d);
        if (!copy.decisions.empty()) out.push_back(copy);
    }
    return out;
}
inline std::vector<Hand> streetOnly(const std::vector<Hand> &hands,
                                    const std::string &street) {
    std::vector<Hand> out;
    out.reserve(hands.size());
    for (const Hand &h : hands) {
        Hand copy;
        copy.session = h.session;
        for (const Decision &d : h.decisions)
            if (d.phase == street) copy.decisions.push_back(d);
        if (!copy.decisions.empty()) out.push_back(copy);
    }
    return out;
}
} // namespace KibitzerTrainingStats
