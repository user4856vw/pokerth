// Local-only, opt-in Kibitzer self-play policy.
// Pure C++ so the action mapping can be tested without Qt or a poker engine.
// Stage A's aggression candidates are NOT unconditional raise instructions.
#pragma once
#include <cmath>

namespace KibitzerAutoplay {

enum class Action { Check, Call, Fold, Raise };
// Council's actual combined stance (not merely its component pot-odds lens).
enum class CouncilStance { Abstain, Check, Call, Fold, Raise, Passive };

struct Input {
    CouncilStance council = CouncilStance::Abstain;
    bool adviceReady = false;
    bool rangesUsable = true;
    int price = 0;
    int minRaise = 0;
    int maxRaise = 0;
    double equity = 0.0;
    double required = 0.0;
    double margin = 0.0;
    bool aggressionCandidate = false;
    bool modeledRaiseEdge = false;
    int modeledRaiseSize = 0;
};

struct Decision {
    Action action;
    int amount;
    bool provisional; // Not an executable Council consensus (or lacks a model).
    const char *reason;
};

// The overnight experiment needs a total policy, whereas Council has several
// explicitly conditional findings. Record every interpolation instead of
// relabelling it as a professional/GTO recommendation.
inline Decision choose(const Input &in)
{
    if (!in.adviceReady || !in.rangesUsable ||
        !std::isfinite(in.equity) || !std::isfinite(in.required) ||
        !std::isfinite(in.margin) || in.margin < 0.0 ||
        in.equity < 0.0 || in.equity > 1.0 ||
        in.required < 0.0 || in.required > 1.0) {
        return {in.price == 0 ? Action::Check : Action::Fold, 0, true,
                "model-unavailable-safe-check-or-fold"};
    }
    if (in.council == CouncilStance::Raise &&
        in.modeledRaiseEdge && in.modeledRaiseSize > 0 &&
        in.minRaise > 0 && in.maxRaise >= in.minRaise &&
        in.modeledRaiseSize >= in.minRaise &&
        in.modeledRaiseSize <= in.maxRaise) {
        return {Action::Raise, in.modeledRaiseSize, false,
                "explicit-conditional-bet-ev-raise"};
    }
    // Stage A is an unpriced suggestion to investigate a raise. During
    // local self-play *only*, try the legal minimum as a marked experiment
    // when no combined Council stance contradicts the aggression signal.
    if (in.council == CouncilStance::Abstain && in.aggressionCandidate &&
        in.minRaise > 0 && in.maxRaise >= in.minRaise) {
        return {Action::Raise, in.minRaise, true,
                "stage-a-unpriced-minimum-raise-experiment"};
    }
    // No free folds, even if a conditional aggression signal exists.
    if (in.price <= 0)
        return {Action::Check, 0, in.council != CouncilStance::Check,
                in.council == CouncilStance::Check
                    ? "council-check" : "safe-free-check-no-council-consensus"};

    if (in.equity > in.required + in.margin)
        return {Action::Call, in.price, in.council != CouncilStance::Call,
                in.council == CouncilStance::Call
                    ? "council-call" : "pot-odds-lens-call-no-council-consensus"};
    if (in.equity < in.required - in.margin)
        return {Action::Fold, 0, in.council != CouncilStance::Fold,
                in.council == CouncilStance::Fold
                    ? "council-fold" : "pot-odds-lens-fold-no-council-consensus"};

    // Council abstains in this band. To keep the trial running, use the
    // raw pot-odds comparison as an *explicitly provisional* tie-break.
    if (in.equity >= in.required)
        return {Action::Call, in.price, true, "close-pot-odds-call-tiebreak"};
    return {Action::Fold, 0, true, "close-pot-odds-fold-tiebreak"};
}

} // namespace KibitzerAutoplay
