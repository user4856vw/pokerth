#include "kibitzer_betsizing_logic.h"
#include <cassert>
#include <cmath>

using namespace KibitzerBetSizing;
static void closeTo(double a, double b) { assert(std::fabs(a-b)<1.e-9); }
static int card(int rank, int suit) { return suit*13 + rank-2; }

int main() {
    {
        // Heads-up preflop: all 1,225 unblocked combinations equally likely.
        int hero[2] = {card(2,0), card(3,1)};
        int board[5] = {-1,-1,-1,-1,-1};
        HoleCardsRange premium("AA");
        auto r=countResponses(nullptr,premium,hero,board,0);
        assert(r.valid && r.subset && r.prior==1225 && r.continueCombos==6);
        closeTo(r.foldProbability,1.0-6.0/1225.0);
        auto all=countAllResponses(nullptr,hero,board,0);
        assert(all.valid && all.prior==1225 && all.continueCombos==1225);
        closeTo(all.foldProbability,0.0);
        board[0] = card(14,0); board[1] = card(7,1); board[2] = card(9,2);
        r=countResponses(nullptr,premium,hero,board,3);
        assert(r.valid && r.prior==1081 && r.continueCombos==3);
    }
    {
        int hero[2] = {card(2,0),card(3,1)};
        int board[5] = {-1,-1,-1,-1,-1};
        HoleCardsRange prior("AA,KK"), call("AA"), impossible("QQ");
        auto r=countResponses(&prior,call,hero,board,0);
        assert(r.valid && r.prior==12 && r.continueCombos==6);
        closeTo(r.foldProbability,0.5);
        const auto all=countAllResponses(&prior,hero,board,0);
        assert(all.valid && all.prior==12 && all.continueCombos==12);
        closeTo(all.foldProbability,0.0);
        r=countResponses(&prior,impossible,hero,board,0);
        assert(!r.valid && !r.subset);
        int blocked[2] = {card(14,0),card(3,1)};
        r=countResponses(&prior,call,blocked,board,0);
        assert(r.valid && r.prior==9 && r.continueCombos==3);
        closeTo(r.foldProbability,2.0/3.0);
    }
    {
        ActionInput a;
        a.pot=100; a.call=0; a.minRaise=20; a.maxRaise=200;
        a.opponentCash=200; a.heroSpend=50;
        a.baselineEquity=.6; a.continuingEquity=.7;
        a.foldProbability=.3;
        auto v=evaluate(a);
        assert(v.valid);
        closeTo(v.checkOrCallEV,60);
        closeTo(v.referenceEV,60);
        closeTo(v.conditionalCallPot,200);
        closeTo(v.raiseEV,93);
        closeTo(v.advantage,33);
        assert(v.opponentAdditionalCall==50);
        // With NO fold equity and weaker calling equity, betting can be worse.
        a.foldProbability=0; a.continuingEquity=.2;
        v=evaluate(a);
        assert(v.valid);
        closeTo(v.raiseEV,-10);
        closeTo(v.advantage,-70);
    }
    {
        // Hero faces a 20-chip call. Their 80-chip raise spends 80 total,
        // and opponent pays only an ADDITIONAL 60 chips if calling.
        ActionInput a;
        a.pot=120; a.call=20; a.minRaise=60; a.maxRaise=200;
        a.opponentCash=100; a.heroSpend=80;
        a.baselineEquity=.4; a.continuingEquity=.4;
        a.foldProbability=.5;
        auto v=evaluate(a);
        assert(v.valid && v.opponentAdditionalCall==60);
        closeTo(v.conditionalCallPot,260);
        closeTo(v.checkOrCallEV,36);
        closeTo(v.referenceEV,36);
        closeTo(v.raiseEV,72);
        closeTo(v.advantage,36);
        a.opponentCash=60; // equal amount -> all-in caller; out of scope
        assert(!evaluate(a).valid);
        a.opponentCash=100; a.heroSpend=200; // hero all-in; out of scope
        assert(!evaluate(a).valid);
        a.heroSpend=40; // below legal minimum
        assert(!evaluate(a).valid);
        a.heroSpend=80; a.foldProbability=1.1; // invalid assumption
        assert(!evaluate(a).valid);
    }
    {
        assert(sizeForPotFraction(100,0,1,2)==50);
        assert(sizeForPotFraction(100,20,1,1)==140); // C + pot-after-call = 20 + 120
        assert(sizeForPotFraction(120,20,1,2)==90);  // C + half of (120+20)
        assert(sizeForPotFraction(0,20,1,2)==0);
        ActionInput a;
        a.pot=100; a.call=60; a.minRaise=100; a.maxRaise=150;
        a.opponentCash=120; a.heroSpend=50;
        assert(!evaluate(a).valid);
    }
    return 0;
}
