#include "kibitzer_mc_config.h"
#include <cassert>
using namespace KibitzerMonteCarlo;
int main() {
    static_assert(kMinRuns == 1000, "MC lower bound");
    static_assert(kMaxRuns == 10000, "MC upper bound");
    static_assert(kDefaultRuns == 2500, "Keep prior APK default");
    assert(valid(1000));
    assert(valid(2500));
    assert(valid(7500));
    assert(valid(10000));
    assert(!valid(999));
    assert(!valid(10001));
    assert(!valid(-1));
    assert(fromStored(1000) == 1000);
    assert(fromStored(10000) == 10000);
    assert(fromStored(2500) == 2500);
    assert(fromStored(999) == kDefaultRuns);
    assert(fromStored(10001) == kDefaultRuns);
    assert(fromStored(0) == kDefaultRuns);
    return 0;
}
