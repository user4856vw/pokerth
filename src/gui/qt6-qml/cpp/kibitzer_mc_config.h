#pragma once
// User-configurable Monte Carlo *per equity scenario* in the local Kibitzer.
// Other PokerTH AI calculations are intentionally unaffected.
namespace KibitzerMonteCarlo {
constexpr int kMinRuns = 1000;
constexpr int kMaxRuns = 10000;
constexpr int kDefaultRuns = 2500;
inline bool valid(int runs) {
    return runs >= kMinRuns && runs <= kMaxRuns;
}
inline int fromStored(int runs) {
    // Fail back to historical default if a preference is malformed,
    // rather than silently switching to the minimum.
    return valid(runs) ? runs : kDefaultRuns;
}
}
