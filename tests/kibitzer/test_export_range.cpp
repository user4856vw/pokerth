#include "kibitzer_export_range.h"
#include <cassert>
#include <iostream>
using namespace KibitzerExport;
int main(){
    const QTimeZone berlin("Europe/Berlin");
    const auto r=parseRange("2026-11-27 21:40","2026-11-28 06:40",berlin);
    assert(r.valid());
    const auto meta=metadata(r);
    assert(meta.value("fromInclusiveUtc").toString()=="2026-11-27T20:40:00Z");
    assert(meta.value("untilExclusiveUtc").toString()=="2026-11-28T05:40:00Z");
    assert(!meta.value("restorable").toBool());
    auto h=[](const char* s){return QJsonObject{{"completedAt",QString::fromLatin1(s)}};};
    assert(match(h("2026-11-27T20:39:59Z"),true,r)==Match::Outside);
    assert(match(h("2026-11-27T20:40:00Z"),true,r)==Match::Inside);
    assert(match(h("2026-11-28T05:39:59.999Z"),true,r)==Match::Inside);
    assert(match(h("2026-11-28T05:40:00Z"),true,r)==Match::Outside);
    assert(match(h("2026-11-27T21:40:00+01:00"),true,r)==Match::Inside);
    assert(match(h("2026-11-27T20:40:00"),true,r)==Match::InvalidTimestamp);
    assert(match({},true,r)==Match::InvalidTimestamp);
    assert(match(QJsonObject{{"finishedAt","2026-11-28T05:00:00Z"}},false,r)==Match::Inside);
    assert(match(h("2026-11-28T05:00:00Z"),false,r)==Match::InvalidTimestamp);
    assert(!parseRange("2026-11-28 06:40","2026-11-27 21:40",berlin).valid());
    assert(!parseRange("2026-11-27 21:40","2026-11-27 21:40",berlin).valid());
    assert(!parseLocalMinute("2026-02-30 12:00",berlin).isValid());
    assert(!parseLocalMinute("2026-03-29 02:30",berlin).isValid()); // missing DST hour
    assert(!parseLocalMinute("2026-10-25 02:30",berlin).isValid()); // repeated DST hour
    assert(parseLocalMinute("2026-07-01 12:00",berlin).toUTC().toString(Qt::ISODate)=="2026-07-01T10:00:00Z");
    assert(parseRange("2026-10-25 01:00","2026-10-25 04:00",berlin).valid());
    std::cout<<"Date range UTC conversion, boundary, metadata and DST tests passed\n";
}
