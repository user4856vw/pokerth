#include "kibitzer_review_logic.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
using namespace KibitzerReviewLogic;

int main() {
    Input a;
    a.action = Action::Fold;
    a.price = 0;
    assert(assess(a).rating == Rating::ClearMistake);
    a.action = Action::CheckOrCall;
    assert(assess(a).rating == Rating::NoFlag);
    a.price = 50;
    a.equityAvailable = true;
    a.samples = 2500;
    a.equity = 0.05;
    a.required = 0.4;
    assert(assess(a).rating == Rating::Review);
    a.manualRanges = true;
    assert(assess(a).rating == Rating::Review);
    assert(std::string(assess(a).reason).find("assigned ranges") != std::string::npos);
    a.manualRanges = false;
    a.allInOrSidePot = true;
    assert(assess(a).rating == Rating::Unscored);
    a.allInOrSidePot = false;
    a.action = Action::Fold;
    a.equity = 0.8;
    assert(assess(a).rating == Rating::Review);
    a.equity = 0.5;
    assert(assess(a).rating == Rating::NoFlag);
    a.action = Action::BetOrRaise;
    assert(assess(a).rating == Rating::Unscored);
    a.action = Action::AllIn;
    assert(assess(a).rating == Rating::Unscored);
    a.action = Action::CheckOrCall;
    a.samples = 4;
    assert(assess(a).rating == Rating::Unscored);
    a.samples = 5000;
    a.equity = NAN;
    assert(assess(a).rating == Rating::Unscored);
    std::cout << "All KibitzerReviewLogic tests passed\n";
}
