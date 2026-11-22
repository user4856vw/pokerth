#pragma once
// PokerTH Kibitzer review classifier; no Qt or poker-engine dependencies.
// Keep quantitative *model warnings* separate from mathematically certain errors.
#include <algorithm>
#include <cmath>

namespace KibitzerReviewLogic {
enum class Action { Fold, CheckOrCall, BetOrRaise, AllIn };
enum class Rating { ClearMistake, Review, NoFlag, Unscored };

struct Input {
    Action action = Action::Fold;
    int price = 0;
    bool equityAvailable = false;
    double equity = 0.0;
    double required = 0.0;
    int samples = 0;
    bool allInOrSidePot = false;
    bool manualRanges = false;
};

struct Assessment {
    Rating rating;
    const char *reason;
};

inline Assessment assess(const Input &in) {
    // Checking preserves the entire free option to fold later. That is true
    // regardless of opponents' ranges, stack depth, or Monte Carlo noise.
    if (in.action == Action::Fold && in.price == 0)
        return {Rating::ClearMistake, "Folded even though checking cost no chips."};
    if (in.action == Action::BetOrRaise || in.action == Action::AllIn)
        return {Rating::Unscored, "Bet sizing, fold equity and tournament value are not modeled."};
    if (in.action == Action::CheckOrCall && in.price == 0)
        return {Rating::NoFlag, "Checked at no cost; future betting is not modeled."};
    if (in.allInOrSidePot)
        return {Rating::Unscored, "All-in/side-pot eligibility requires a pot-specific EV model."};
    if (!in.equityAvailable || in.samples < 1000 || !std::isfinite(in.equity) ||
        !std::isfinite(in.required) || in.required < 0.0 || in.required > 1.0 ||
        in.equity < 0.0 || in.equity > 1.0)
        return {Rating::Unscored, "Not enough reliable information for a comparison."};

    // A wide, deliberately conservative statistical guard. It accounts only
    // for sampling error, NOT for the much larger uncertainty in hand ranges.
    const double epsilon = std::sqrt(std::log(40.0) / (2.0 * in.samples));
    const double gap = std::max(0.16, 2.0 * epsilon);
    if (in.action == Action::CheckOrCall && in.equity + gap < in.required)
        return {Rating::Review, in.manualRanges
            ? "Expensive call under your assigned ranges; assumptions and future betting may differ."
            : "Expensive call versus random opponents; actual betting ranges may differ."};
    if (in.action == Action::Fold && in.equity > in.required + gap)
        return {Rating::Review, in.manualRanges
            ? "Folded despite favorable pot odds under your assigned ranges; these are only assumptions."
            : "Folded despite favorable random-hand pot odds; a strong range can justify it."};
    return {Rating::NoFlag, "No clear discrepancy under the simplified model."};
}
} // namespace KibitzerReviewLogic
