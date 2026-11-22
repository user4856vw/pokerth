#pragma once
// Tournament chip accounting. Pure C++ so blind normalization can be regression-tested.
// Observed hand: stack AFTER payout minus stack BEFORE blinds/antes are posted.
// No inference of missing older hands, no ICM and no all-in-adjustment.
#include <algorithm>
#include <cmath>
#include <vector>

namespace KibitzerTournamentStats {

struct HandResult {
    int startingChips = 0;
    int endingChips = 0;
    int bigBlind = 0;
    bool valid = false;
};
struct Finish {
    int entrants = 0;
    int place = 0;        // 1 = win; 0 means an abandoned/incomplete event
    bool completed = false;
};
struct Summary {
    int hands = 0;
    long long netChips = 0;
    double totalBb = 0.0;
    double bbPer100 = 0.0;
    int completedTournaments = 0;
    int wins = 0;
    long long placingSum = 0;
    double averagePlace = 0.0;
    double averageNormalizedPlace = 0.0; // 0 = first, 1 = last
    int abandonedTournaments = 0;
};

inline bool admissible(const HandResult &h) {
    return h.valid && h.bigBlind > 0 && h.startingChips >= 0
           && h.endingChips >= 0;
}
inline void addHand(Summary &s, const HandResult &h) {
    if (!admissible(h)) return;
    ++s.hands;
    s.netChips += static_cast<long long>(h.endingChips) - h.startingChips;
    s.totalBb += double(h.endingChips - h.startingChips) / h.bigBlind;
    s.bbPer100 = 100.0 * s.totalBb / s.hands;
}
inline void addFinish(Summary &s, const Finish &f) {
    if (!f.completed) {
        ++s.abandonedTournaments;
        return;
    }
    if (f.entrants < 2 || f.place < 1 || f.place > f.entrants) return;
    ++s.completedTournaments;
    s.placingSum += f.place;
    if (f.place == 1) ++s.wins;
    s.averagePlace = double(s.placingSum) / s.completedTournaments;
}
inline Summary summarize(const std::vector<HandResult> &hands,
                         const std::vector<Finish> &finishes) {
    Summary out;
    double normalizedSum = 0.0;
    for (const auto &h : hands) addHand(out,h);
    for (const auto &f : finishes) {
        if (f.completed && f.entrants >= 2 &&
            f.place >= 1 && f.place <= f.entrants)
            normalizedSum += double(f.place-1)/(f.entrants-1);
        addFinish(out,f);
    }
    if (out.completedTournaments)
        out.averageNormalizedPlace=normalizedSum/out.completedTournaments;
    return out;
}
}
