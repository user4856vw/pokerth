// Non-destructive range selection for analysis exports. Never used to
// mutate the durable journal. Qt interprets the input in the device zone.
#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QTimeZone>
#include <QString>

namespace KibitzerExport {
struct Range {
    QDateTime from,until;
    bool valid() const { return from.isValid() && until.isValid() && from<until; }
};
inline QDateTime parseLocalMinute(const QString& input,
                                 const QTimeZone& zone=QTimeZone::systemTimeZone()){
    // Strict minute precision; reject normalized/ambiguous local wall time.
    const QString fmt=QStringLiteral("yyyy-MM-dd HH:mm");
    const auto text=input.trimmed();
    if(text.size()!=16)return {};
    const auto date=QDate::fromString(text.left(10),QStringLiteral("yyyy-MM-dd"));
    const auto time=QTime::fromString(text.mid(11),QStringLiteral("HH:mm"));
    if(!date.isValid()||!time.isValid()||!zone.isValid())return {};
    // Parse as a neutral UTC wall clock before converting to the device zone.
    const QDateTime wall(date,time,QTimeZone(QByteArray("UTC")));
    if(wall.toString(fmt)!=text)return {};
    const auto before=wall.addDays(-1),after=wall.addDays(1);
    const int offsets[]={zone.offsetFromUtc(before),zone.offsetFromUtc(wall),
                         zone.offsetFromUtc(after)};
    QDateTime result;
    for(int offset:offsets){
        const auto candidate=wall.addSecs(-offset);
        if(candidate.toTimeZone(zone).toString(fmt)!=text)continue;
        if(result.isValid()&&result!=candidate)return {};
        result=candidate;
    }
    return result;
}
inline Range parseRange(const QString& from,const QString& until,
                        const QTimeZone& zone=QTimeZone::systemTimeZone()){
    return {parseLocalMinute(from,zone),parseLocalMinute(until,zone)};
}
enum class Match { InvalidTimestamp, Outside, Inside };
inline Match match(const QJsonObject& value,bool hand,const Range& range){
    const QString raw=value.value(hand?QStringLiteral("completedAt"):
                                        QStringLiteral("finishedAt")).toString();
    // The durable journal writes ISO UTC. Explicit offset timestamps are
    // accepted too; reject naive local timestamps rather than guess a zone.
    const bool offset=raw.endsWith(QLatin1Char('Z'))||
        (raw.size()>=25&&(raw.at(raw.size()-6)==QLatin1Char('+')||
                         raw.at(raw.size()-6)==QLatin1Char('-'))&&
                         raw.at(raw.size()-3)==QLatin1Char(':'));
    const auto time=QDateTime::fromString(raw,Qt::ISODate);
    if(!range.valid()||!offset||!time.isValid())return Match::InvalidTimestamp;
    return time>=range.from&&time<range.until?Match::Inside:Match::Outside;
}
inline QJsonObject metadata(const Range& r){
    return {{QStringLiteral("kind"),QStringLiteral("time-range")},
            {QStringLiteral("fromInclusiveUtc"),r.from.toUTC().toString(Qt::ISODate)},
            {QStringLiteral("untilExclusiveUtc"),r.until.toUTC().toString(Qt::ISODate)},
            {QStringLiteral("handSelection"),QStringLiteral("completedAt")},
            {QStringLiteral("tournamentSelection"),QStringLiteral("finishedAt")},
            {QStringLiteral("restorable"),false}};
}
} // namespace KibitzerExport
