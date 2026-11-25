#include "kibitzer_evidence_open_ranges.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace KibitzerEvidence;
using namespace KibitzerPreflopRanges;

int main() {
    // Every one of the 1,326 physical hole-card deals must decode into
    // exactly one class, and the class sizes must be 6/4/12.
    int counts[169]={0};
    for(int a=0;a<52;++a)for(int b=a+1;b<52;++b) {
        const int cls=handClassIndex(decode(a,b));
        assert(cls>=0 && cls<169);
        ++counts[cls];
    }
    for(int i=0;i<169;++i)
        assert(counts[i]==(i<13?6:i<91?4:12));

    // Separate 6/8/9-handed positions; never invent a 7- or 10-max chart.
    assert(positionIndex(9,3)==3);  // EP1
    assert(positionIndex(9,8)==8);  // CO
    assert(positionIndex(8,3)==4);  // EP2
    assert(positionIndex(8,7)==8);  // CO
    assert(positionIndex(6,3)==6);  // LJ
    assert(positionIndex(6,5)==8);  // CO
    assert(positionIndex(10,3)==-1);
    assert(positionIndex(7,3)==-1);
    assert(positionIndex(3,0)==-1);
    assert(positionIndex(2,0)==-1);
    assert(positionIndex(9,2)==-1); // BB cannot open first-in

    // Source snapshots record pure and mixed frequencies (not a binary
    // solver verdict). Pocket aces must be an opening hand in these models.
    for (int alive : {6,8,9}) for(int bb : {5,10,20,40,100}) {
        const auto aa=lookup(12,25,alive,0,bb,false,0,0);
        assert(aa.applicable && aa.frequencyThousandths==1000);
        assert(std::strcmp(aa.referencePosition,"BTN")==0);
        assert(std::strcmp(aa.status,
                           "external-opening-chart-reference")==0);
    }
    // 7d2c is not a 5BB EP1 open under the pinned model.
    const auto trash=lookup(5,39,9,3,5,false,0,0);
    assert(trash.applicable && trash.frequencyThousandths==0);

    // This NEVER uses the wrong source context just to provide a verdict.
    assert(!lookup(12,25,10,0,5,false,0,0).applicable);
    assert(!lookup(12,25,7,0,5,false,0,0).applicable);
    assert(!lookup(12,25,2,0,5,false,0,0).applicable);
    assert(!lookup(12,25,9,0,0.87,false,0,0).applicable); // KTo crisis
    assert(!lookup(12,25,9,0,9.0,false,0,0).applicable);
    assert(!lookup(12,25,9,0,5,true,0,0).applicable);
    assert(!lookup(12,25,9,0,5,false,1,0).applicable);
    assert(!lookup(12,25,9,0,5,false,0,1).applicable);
    assert(!lookup(-1,25,9,0,5,false,0,0).applicable);
    assert(!lookup(12,12,9,0,5,false,0,0).applicable);
    const auto d=lookup(50,21,9,0,5,false,0,0); // Kc Th
    assert(d.applicable && d.frequencyThousandths>=0 &&
           d.frequencyThousandths<=1000);
    assert(d.referenceBB==5);

    // No unreviewed reference frequency is an instruction to the agent.
    // The existing choose(Input) is not called by any lookup path.
    std::cout << "evidence charts: 1326 deals, 169 classes, source gating passed\n";
}
