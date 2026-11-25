// Derive *tournament* preflop position from the actual surviving seat ring.
// getMyButton()==0 only means "not currently holding a blind/dealer marker";
// it is NOT evidence of early position. All-in/folded players in this hand
// remain in the ring until eliminated; no hole cards are inspected.
#pragma once
#include <algorithm>
#include <vector>
namespace KibitzerPreflopPosition {
struct Seat { int id; int button; bool alive; };
struct Result { int alive=0; int heroRole=-1; int dealerId=-1; };
// Role indices clockwise from dealer: 0 BTN, 1 SB, 2 BB,
// 3 first-to-act (UTG) in >=4-handed games.
// Heads-up: 0 is button/SB and 1 is BB.
inline Result derive(const std::vector<Seat> &seats,int heroId=0) {
    Result out;
    int physicalSeats=0;
    for (const Seat &s:seats)
        if (s.id>=0) physicalSeats=std::max(physicalSeats,s.id+1);
    if (physicalSeats<2 || physicalSeats>10) return out;
    std::vector<int> ring;
    for (const Seat &s:seats) {
        if (!s.alive) continue;
        ring.push_back(s.id);
        if (s.button==1) out.dealerId=s.id;
    }
    out.alive=static_cast<int>(ring.size());
    if (out.alive<2 || out.dealerId<0) return out;
    // Do not rely on underlying PlayerList ordering: this engine keeps
    // fixed seat IDs, while active player lists change as people bust.
    std::sort(ring.begin(),ring.end(),[&](int a,int b) {
        const int da=(a-out.dealerId+physicalSeats)%physicalSeats;
        const int db=(b-out.dealerId+physicalSeats)%physicalSeats;
        return da<db;
    });
    const auto found=std::find(ring.begin(),ring.end(),heroId);
    if (found!=ring.end())
        out.heroRole=static_cast<int>(found-ring.begin());
    return out;
}
} // namespace KibitzerPreflopPosition
