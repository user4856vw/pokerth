#include "kibitzer_training_stats.h"
#include <cassert>
#include <cmath>
#include <vector>
using namespace KibitzerTrainingStats;
static void near(double a,double b) {assert(std::fabs(a-b)<1e-8);}
int main() {
    std::vector<Hand> hs = {
        {1, {{"Preflop","CHECK/CALL","NO FLAG",20,false,"BTN"},
             {"Flop","CHECK/CALL","NO FLAG",0,false,"BTN"},
             {"Turn","FOLD","CLEAR MISTAKE",0,false,"BTN"}}},
        {1, {{"Preflop","BET/RAISE","NOT GRADED",20,true,"SB"},
             {"Flop","BET/RAISE","NOT GRADED",0,true,"SB"},
             {"River","CHECK/CALL","REVIEW",100,false,"BTN"}}},
        {2, {{"Preflop","CHECK/CALL","NO FLAG",0,false,"BTN"},
             {"Flop","FOLD","REVIEW",25,false,"Other"}}},
        {2, {{"Preflop","ALL-IN","NOT GRADED",40,false,"BTN"}}},
        {3, {}} // no action: never pretend the hand was monitored
    };
    const Summary s=summarize(hs,0,hs.size());
    assert(s.hands==4 && s.decisions==9);
    assert(s.preflopObservedHands==4 && s.vpipHands==3 && s.pfrHands==1);
    assert(s.freeCheckOpportunities==4 && s.clearMistakes==1);
    assert(s.pricedReviewed==3 && s.reviewWarnings==2);
    assert(s.postflopAggressive==1 && s.postflopPassive==2);
    near(percent(s.clearMistakes,s.freeCheckOpportunities),25.0);
    near(percent(s.reviewWarnings,s.pricedReviewed),200.0/3.0);
    near(percent(0,0),0.0);
    const Summary last=summarize(hs,2,5);
    assert(last.hands==2 && last.reviewWarnings==1 && last.clearMistakes==0);
    const auto button=preflopPositionOnly(hs,"BTN");
    assert(button.size()==3 && summarize(button,0,button.size()).vpipHands==2);
    const auto river=streetOnly(hs,"River");
    assert(river.size()==1 && summarize(river,0,river.size()).pricedReviewed==1);
    const auto flop=streetOnly(hs,"Flop");
    assert(flop.size()==3 && summarize(flop,0,flop.size()).reviewWarnings==1);
    return 0;
}
