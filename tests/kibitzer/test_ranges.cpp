// Regression checks for manually assigned opponent starting-hand ranges.
// Standalone test: no Qt, poker game or Android runtime required.
#include "holecardsrange.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
void approx(double a, double b) { assert(std::fabs(a - b) < 1e-12); }
}

int main() {
    HoleCardsRange pairs;
    assert(pairs.add("77+"));
    // 77, 88, 99, TT, JJ, QQ, KK, AA: eight ranks, six combos each.
    approx(pairs.combinationShare(), 48.0 / 1326.0);
    assert(pairs.contains(5, 18));   // 7 diamonds and 7 hearts
    assert(!pairs.contains(0, 13));  // 2 diamonds and 2 hearts

    HoleCardsRange precise;
    assert(precise.add("AA,AKs,AJo"));
    approx(precise.combinationShare(), 22.0 / 1326.0);
    assert(precise.contains(12, 25)); // AA
    assert(precise.contains(12, 11)); // A-K suited (diamonds)
    assert(!precise.contains(12, 24)); // A-K offsuit excluded
    assert(precise.contains(12, 22));  // A-J offsuit

    HoleCardsRange allAK;
    assert(allAK.add("AK"));
    approx(allAK.combinationShare(), 16.0 / 1326.0);

    HoleCardsRange interval;
    assert(interval.add("KTo-K7o"));
    approx(interval.combinationShare(), 4 * 12.0 / 1326.0);
    assert(interval.contains(11, 18)); // K diamond, 7 heart
    assert(!interval.contains(11, 5)); // K diamond, 7 diamond: suited

    HoleCardsRange invalid;
    assert(!invalid.add("AKx"));
    assert(invalid.empty());

    HoleCardsRange empty;
    assert(empty.empty());
    assert(empty.add(""));
    assert(empty.empty());

    std::cout << "manual opponent range parsing passed\n";
}
