// PokerTH Kibitzer v3 — private, persistent training history.
// No network access and no reading of opponents' unrevealed cards.
#include "gamehandler.h"
#include "kibitzer_training_stats.h"
#include "kibitzer_tournament_stats.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>
#include <vector>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace {
constexpr int kSchema = 1;
constexpr int kMaxHands = 2500;
constexpr int kMaxTournaments = 1000;
constexpr int kMaxBytes = 64 * 1024 * 1024; // legacy imports, NOT journal lifetime
QString journalPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/kibitzer_training_v2.jsonl");
}
QByteArray journalLine(const QString &type, const QVariantMap &value) {
    const QJsonObject object{
        {QStringLiteral("type"),type},
        {QStringLiteral("value"),QJsonObject::fromVariantMap(value)}
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}
QString statsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/kibitzer_training_v1.json");
}
QJsonObject makeArchive(const QVariantList &hands, const QVariantList &tournaments, int lastSession) {
    QJsonObject out;
    out[QStringLiteral("schemaVersion")] = kSchema;
    out[QStringLiteral("lastSessionId")] = lastSession;
    out[QStringLiteral("hands")] = QJsonArray::fromVariantList(hands);
    out[QStringLiteral("tournaments")] = QJsonArray::fromVariantList(tournaments);
    return out;
}
// Fail closed on incompatible or corrupt data, rather than quietly
// overwriting an older archive with a new empty one.
bool validateArchive(const QJsonDocument &doc) {
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt(-1) != kSchema ||
        !root.value(QStringLiteral("hands")).isArray()) return false;
    const auto hands = root.value(QStringLiteral("hands")).toArray();
    if (hands.size() > 50000) return false; // bounded import, memory later trimmed
    if (root.contains(QStringLiteral("tournaments"))) {
        if (!root.value(QStringLiteral("tournaments")).isArray()) return false;
        const auto tournaments = root.value(QStringLiteral("tournaments")).toArray();
        if (tournaments.size() > 10000) return false;
        for (const auto &item : tournaments) {
            if (!item.isObject()) return false;
            const QJsonObject t = item.toObject();
            if (!t.value(QStringLiteral("completed")).isBool() ||
                !t.value(QStringLiteral("entrants")).isDouble() ||
                !t.value(QStringLiteral("place")).isDouble()) return false;
            const int e = t.value(QStringLiteral("entrants")).toInt(-1);
            const int p = t.value(QStringLiteral("place")).toInt(-1);
            if (e < 2 || e > 10 || p < 0 || p > e ||
                (t.value(QStringLiteral("completed")).toBool() && p == 0)) return false;
        }
    }
    for (const auto &item : hands) {
        if (!item.isObject()) return false;
        const QJsonObject hand = item.toObject();
        if (!hand.value(QStringLiteral("entries")).isArray() ||
            !hand.value(QStringLiteral("session")).isDouble()) return false;
        const QJsonArray entries = hand.value(QStringLiteral("entries")).toArray();
        if (entries.size() > 64) return false;
        for (const auto &entry : entries) {
            if (!entry.isObject()) return false;
            const auto d = entry.toObject();
            if (!d.value(QStringLiteral("phase")).isString() ||
                !d.value(QStringLiteral("action")).isString() ||
                !d.value(QStringLiteral("rating")).isString()) return false;
        }
    }
    return true;
}
KibitzerTrainingStats::Summary summaryOf(const std::vector<KibitzerTrainingStats::Hand>& hands,
                                          std::size_t from, std::size_t to) {
    return KibitzerTrainingStats::summarize(hands, from, to);
}
QVariantMap describe(const KibitzerTrainingStats::Summary &s) {
    using KibitzerTrainingStats::percent;
    QVariantMap m;
    m["hands"] = s.hands;
    m["decisions"] = s.decisions;
    m["folds"] = s.folds;
    m["checks"] = s.checks;
    m["calls"] = s.calls;
    m["raises"] = s.raises;
    m["allIns"] = s.allIns;
    m["freeOpportunities"] = s.freeCheckOpportunities;
    m["clearMistakes"] = s.clearMistakes;
    m["freeFoldPercent"] = percent(s.clearMistakes,s.freeCheckOpportunities);
    m["pricedReviewed"] = s.pricedReviewed;
    m["reviewWarnings"] = s.reviewWarnings;
    m["reviewPercent"] = percent(s.reviewWarnings,s.pricedReviewed);
    m["ungraded"] = s.ungraded;
    m["preflopObservedHands"] = s.preflopObservedHands;
    m["vpipHands"] = s.vpipHands;
    m["vpipPercent"] = percent(s.vpipHands,s.preflopObservedHands);
    m["pfrHands"] = s.pfrHands;
    m["pfrPercent"] = percent(s.pfrHands,s.preflopObservedHands);
    m["postflopAggressive"] = s.postflopAggressive;
    m["postflopPassive"] = s.postflopPassive;
    m["postflopAggressionPercent"] = percent(s.postflopAggressive,
                                              s.postflopAggressive+s.postflopPassive);
    return m;
}
std::vector<KibitzerTrainingStats::Hand> convertHands(const QVariantList &list) {
    std::vector<KibitzerTrainingStats::Hand> out;
    out.reserve(static_cast<std::size_t>(list.size()));
    for (const QVariant &value : list) {
        const QVariantMap saved = value.toMap();
        KibitzerTrainingStats::Hand hand;
        hand.session = saved.value("session").toInt();
        for (const QVariant &v : saved.value("entries").toList()) {
            const QVariantMap e = v.toMap();
            KibitzerTrainingStats::Decision d;
            d.phase = e.value("phase").toString().toStdString();
            d.action = e.value("action").toString().toStdString();
            d.rating = e.value("rating").toString().toStdString();
            d.price = e.value("price").toInt();
            d.aggressive = e.value("aggressive").toBool();
            d.position = e.value("position").toString().toStdString();
            hand.decisions.push_back(d);
        }
        out.push_back(std::move(hand));
    }
    return out;
}
} // namespace

QVariantMap GameHandler::kibitzerProgress() const { return m_kibitzerProgress; }

// Version 2: each settled hand and tournament is independently appended as
// one JSONL record. No entire-archive rewrite per hand, no 8-MiB silent ceiling.
// The last 2500 hands are kept in RAM for the GUI; ALL journal records remain
// on disk and are included in the Android export.
void GameHandler::loadKibitzerProgress() {
    m_kibitzerHistory.clear();
    m_kibitzerTournaments.clear();
    m_kibitzerSessionId = 1;
    m_kibitzerHistoryWritable = true;
    m_kibitzerStorageError.clear();
    const QString path = journalPath();
    QFile journal(path);

    if (journal.exists()) {
        if (!journal.open(QIODevice::ReadOnly)) {
            m_kibitzerHistoryWritable = false;
            m_kibitzerStorageError = QStringLiteral("Cannot open v2 history journal; original protected.");
        } else {
            int previous = 0;
            while (!journal.atEnd()) {
                const QByteArray line = journal.readLine();
                if (!line.endsWith('\n')) {
                    m_kibitzerHistoryWritable = false;
                    m_kibitzerStorageError = QStringLiteral(
                        "Incomplete journal tail; original protected. Export or repair before continuing.");
                    break;
                }
                QJsonParseError error;
                const QJsonDocument document = QJsonDocument::fromJson(line,&error);
                const QJsonObject root = document.object();
                const QString type = root.value(QStringLiteral("type")).toString();
                const QJsonObject value = root.value(QStringLiteral("value")).toObject();
                const bool good = error.error==QJsonParseError::NoError &&
                    document.isObject() &&
                    ((type==QLatin1String("hand") &&
                      value.value(QStringLiteral("session")).isDouble() &&
                      value.value(QStringLiteral("entries")).isArray()) ||
                     (type==QLatin1String("tournament") &&
                      value.value(QStringLiteral("session")).isDouble() &&
                      value.value(QStringLiteral("completed")).isBool()));
                if (!good) {
                    m_kibitzerHistoryWritable=false;
                    m_kibitzerStorageError=QStringLiteral(
                        "Malformed v2 journal record; original protected.");
                    break;
                }
                previous = std::max(previous, value.value(QStringLiteral("session")).toInt());
                if (type==QLatin1String("hand")) {
                    m_kibitzerHistory.append(value.toVariantMap());
                    if (m_kibitzerHistory.size()>kMaxHands)
                        m_kibitzerHistory.removeFirst();
                } else {
                    m_kibitzerTournaments.append(value.toVariantMap());
                    if (m_kibitzerTournaments.size()>kMaxTournaments)
                        m_kibitzerTournaments.removeFirst();
                }
            }
            m_kibitzerSessionId = std::max(1,previous+1);
        }
        rebuildKibitzerProgress();
        return;
    }

    // Backwards-compatible import of the old 8-MiB whole-archive snapshot.
    // Never remove/overwrite it during migration; it remains a recovery copy.
    QFile legacy(statsPath());
    if (legacy.exists()) {
        if (!legacy.open(QIODevice::ReadOnly) || legacy.size()>kMaxBytes) {
            m_kibitzerHistoryWritable=false;
            m_kibitzerStorageError=QStringLiteral(
                "Cannot read legacy training archive; original protected.");
        } else {
            const QByteArray bytes=legacy.readAll();
            QJsonParseError parse;
            QJsonDocument doc=QJsonDocument::fromJson(bytes,&parse);
            if (parse.error!=QJsonParseError::NoError || !validateArchive(doc)) {
                m_kibitzerHistoryWritable=false;
                m_kibitzerStorageError=QStringLiteral(
                    "Training archive invalid; original protected.");
            } else {
                const QJsonObject root=doc.object();
                m_kibitzerHistory=root.value(QStringLiteral("hands")).toArray().toVariantList();
                m_kibitzerTournaments=root.value(QStringLiteral("tournaments")).toArray().toVariantList();
                int previous=root.value(QStringLiteral("lastSessionId")).toInt();
                for (const QVariant &v:m_kibitzerHistory)
                    previous=std::max(previous,v.toMap().value("session").toInt());
                for (const QVariant &v:m_kibitzerTournaments)
                    previous=std::max(previous,v.toMap().value("session").toInt());
                m_kibitzerSessionId=std::max(1,previous+1);
                // Publish the migrated complete archive atomically before
                // accepting any additional hand or tournament records.
                saveKibitzerProgress();
                while (m_kibitzerHistory.size()>kMaxHands)
                    m_kibitzerHistory.removeFirst();
                while (m_kibitzerTournaments.size()>kMaxTournaments)
                    m_kibitzerTournaments.removeFirst();
            }
        }
    }
    rebuildKibitzerProgress();
}

// Atomic journal rewrite ONLY for first migration or user-confirmed import.
// Live gameplay uses appendKibitzerJournalRecord instead.
void GameHandler::saveKibitzerProgress() {
    if (!m_kibitzerHistoryWritable) return;
    const QString path=journalPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        m_kibitzerStorageError=QStringLiteral("Cannot create history journal directory.");
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        m_kibitzerStorageError=QStringLiteral("Cannot create history journal.");
        return;
    }
    auto write = [&file](const QString &type,const QVariantList &list) {
        for (const QVariant &value:list) {
            const QByteArray line=journalLine(type,value.toMap());
            if (file.write(line)!=line.size()) return false;
        }
        return true;
    };
    if (!write(QStringLiteral("hand"),m_kibitzerHistory) ||
        !write(QStringLiteral("tournament"),m_kibitzerTournaments) ||
        !file.commit()) {
        file.cancelWriting();
        m_kibitzerStorageError=QStringLiteral("Cannot atomically write history journal.");
        return;
    }
    m_kibitzerStorageError.clear();
}

bool GameHandler::appendKibitzerJournalRecord(const QString &type, const QVariantMap &value) {
    if (!m_kibitzerHistoryWritable) return false;
    const QString path=journalPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        m_kibitzerStorageError=QStringLiteral("Cannot create history journal directory.");
        m_kibitzerHistoryWritable=false;
        return false;
    }
    QFile file(path);
    const QByteArray line=journalLine(type,value);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Append) ||
        file.write(line)!=line.size() || !file.flush()) {
        m_kibitzerStorageError=QStringLiteral(
            "History journal append failed; stop trial and preserve original.");
        m_kibitzerHistoryWritable=false;
        return false;
    }
    m_kibitzerStorageError.clear();
    return true;
}

void GameHandler::persistKibitzerHand() {
    if (!m_kibitzerHistoryWritable ||
        (m_kibitzerEvents.isEmpty() && m_kibitzerHandSettlement.isEmpty())) return;
    QVariantMap hand=m_kibitzerLastReview;
    hand["session"]=m_kibitzerSessionId;
    hand["completedAt"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    // Write to the durable journal BEFORE adding to the in-memory window.
    if (!appendKibitzerJournalRecord(QStringLiteral("hand"),hand)) {
        rebuildKibitzerProgress();
        return;
    }
    m_kibitzerHistory.append(hand);
    while (m_kibitzerHistory.size()>kMaxHands)
        m_kibitzerHistory.removeFirst();
    rebuildKibitzerProgress();
}

void GameHandler::rebuildKibitzerProgress() {
    using KibitzerTrainingStats::Hand;
    // Training action windows count only hands with recorded decisions.
    // Chip BB/100 has a DIFFERENT denominator: every settled dealt hand,
    // including blinds/walks where the human never had to click a button.
    QVariantList decisionsOnly;
    for (const QVariant &v : m_kibitzerHistory)
        if (!v.toMap().value("entries").toList().isEmpty()) decisionsOnly.append(v);
    const auto hands = convertHands(decisionsOnly);
    const std::size_t n = hands.size();
    const auto all = describe(summaryOf(hands,0,n));
    const std::size_t start20 = n > 20 ? n-20 : 0;
    const std::size_t prev20 = start20 > 20 ? start20-20 : 0;
    const std::size_t start50 = n > 50 ? n-50 : 0;
    const std::size_t prev50 = start50 > 50 ? start50-50 : 0;
    QVariantMap result;
    result["totalHands"] = static_cast<int>(n);
    result["all"] = all;
    result["last20"] = describe(summaryOf(hands,start20,n));
    result["previous20"] = describe(summaryOf(hands,prev20,start20));
    result["last50"] = describe(summaryOf(hands,start50,n));
    result["previous50"] = describe(summaryOf(hands,prev50,start50));
    result["canCompare20"] = n >= 40;
    result["canCompare50"] = n >= 100;
    int currentSessionHands = 0;
    for (const Hand &h : hands) if (h.session == m_kibitzerSessionId) ++currentSessionHands;
    result["sessionHands"] = currentSessionHands;
    result["sessionId"] = m_kibitzerSessionId;
    result["storageError"] = m_kibitzerStorageError;
    result["recordedSettledHands"] = static_cast<int>(m_kibitzerHistory.size());
    result["historyLimit"] = kMaxHands;

    QVariantList streetRows;
    for (const char *street : {"Preflop","Flop","Turn","River"}) {
        const auto a = KibitzerTrainingStats::streetOnly(hands,street);
        const auto recent = KibitzerTrainingStats::streetOnly(
             std::vector<Hand>(hands.begin()+static_cast<std::ptrdiff_t>(start50),hands.end()),street);
        const auto previous = KibitzerTrainingStats::streetOnly(
             std::vector<Hand>(hands.begin()+static_cast<std::ptrdiff_t>(prev50),
                               hands.begin()+static_cast<std::ptrdiff_t>(start50)),street);
        QVariantMap item;
        item["street"] = QString::fromLatin1(street);
        item["all"] = describe(summaryOf(a,0,a.size()));
        item["last50"] = describe(summaryOf(recent,0,recent.size()));
        item["previous50"] = describe(summaryOf(previous,0,previous.size()));
        streetRows.append(item);
    }
    result["streets"] = streetRows;
    int maxClear = 0;
    QString priority;
    for (const QVariant &v : streetRows) {
        const QVariantMap m=v.toMap();
        const int count=m.value("all").toMap().value("clearMistakes").toInt();
        if (count>maxClear) {
            maxClear=count;
            priority=m.value("street").toString();
        }
    }
    if (maxClear>0) {
        result["trainingFocus"] = QStringLiteral("Focus on %1: %2 free-check folds recorded. "
             "A free check is always preferable to folding at zero additional cost.")
             .arg(priority).arg(maxClear);
    } else {
        result["trainingFocus"] = QStringLiteral("No free-check folds recorded. "
             "Next step: review opponent ranges and bet-sizing decisions; the present model cannot grade those reliably.");
    }
    QVariantList positions;
    for (const char *pos : {"BTN", "SB", "BB", "Other"}) {
        const auto selected=KibitzerTrainingStats::preflopPositionOnly(hands,pos);
        QVariantMap row=describe(summaryOf(selected,0,selected.size()));
        row["position"]=QString::fromLatin1(pos);
        positions.append(row);
    }
    result["positions"]=positions;

    // Chronological 10-hand buckets; the last bucket may contain <10 hands.
    QVariantList buckets;
    const std::size_t firstBucket = n > 120 ? n-120 : 0;
    for (std::size_t i=firstBucket;i<n;i+=10) {
        QVariantMap b = describe(summaryOf(hands,i,std::min(i+10,n)));
        b["from"] = static_cast<int>(i+1);
        b["to"] = static_cast<int>(std::min(i+10,n));
        buckets.append(b);
    }
    result["buckets"] = buckets;

    QVariantList sessions;
    std::size_t end=n;
    while (end>0 && sessions.size()<8) {
        std::size_t start=end-1;
        const int id=hands[start].session;
        while (start>0 && hands[start-1].session==id) --start;
        QVariantMap item=describe(summaryOf(hands,start,end));
        item["id"]=id;
        item["fromDate"]=decisionsOnly.at(static_cast<int>(start)).toMap().value("completedAt");
        item["toDate"]=decisionsOnly.at(static_cast<int>(end-1)).toMap().value("completedAt");
        sessions.append(item);
        end=start;
    }
    result["sessions"] = sessions;

    // A rolling, strictly local 30-day historical-evidence field trial.
    // Count *decisions*, not refreshes of a popup or Monte Carlo calls.
    // Source data are deliberately tiny: agreement is a descriptive count,
    // NOT an estimate of expert correctness or a win-rate predictor.
    const QDateTime nowUtc=QDateTime::currentDateTimeUtc();
    const QDateTime cutoff=nowUtc.addDays(-30);
    QDateTime firstTracked;
    int checked=0, withAnalogue=0, withoutAnalogue=0, agreeing=0, comparableActions=0;
    QSet<QString> distinctExamples;
    for (const QVariant &v : m_kibitzerHistory) {
        const QVariantMap hand=v.toMap();
        const QDateTime timestamp=QDateTime::fromString(
            hand.value("completedAt").toString(),Qt::ISODate);
        if (!timestamp.isValid() || timestamp<cutoff || timestamp>nowUtc) continue;
        for (const QVariant &actionValue : hand.value("entries").toList()) {
            const QVariantMap evidence=actionValue.toMap()
                .value("historicalEvidence").toMap();
            if (evidence.value("catalogId").toString() !=
                    QLatin1String("wsop2023-43-day5-nlhe")) continue;
            ++checked;
            if (!firstTracked.isValid() || timestamp<firstTracked) firstTracked=timestamp;
            if (evidence.value("closeMatch").toBool()) {
                ++withAnalogue;
                const QString id=evidence.value("matchId").toString();
                if (!id.isEmpty()) distinctExamples.insert(id);
                if (evidence.value("actionComparable").toBool()) {
                    ++comparableActions;
                    if (evidence.value("sameAction").toBool()) ++agreeing;
                }
            } else ++withoutAnalogue;
        }
    }
    QVariantMap pilot;
    pilot["periodDays"]=30;
    pilot["decisionsChecked"]=checked;
    pilot["structuralMatches"]=withAnalogue;
    pilot["noCloseMatch"]=withoutAnalogue;
    pilot["distinctHistoricalExamples"]=distinctExamples.size();
    pilot["comparableActionPairs"]=comparableActions;
    pilot["sameObservedAction"]=agreeing;
    pilot["daysSinceFirstDecision"]=firstTracked.isValid()
        ? int(firstTracked.daysTo(nowUtc)) : 0;
    pilot["firstRecordedAt"]=firstTracked.isValid()
        ? firstTracked.toString(Qt::ISODate) : QString();
    pilot["catalogHands"]=11;
    pilot["catalogDecisions"]=88;
    pilot["explanation"]=QStringLiteral(
        "Rolling last 30 days, recorded after each local decision. "
        "Close means structural match only. Same action is descriptive, NOT "
        "accuracy, optimality, a professional endorsement, or independent votes. "
        "Small, final-table-biased corpus; no data uploaded.");
    result["historicalPilot"]=pilot;

    // BB accounting uses settled-hand evidence, NOT the decision-only filter.
    // A BB is fixed for this individual hand; sum normalized differences
    // before dividing by the number of eligible dealt hands.
    std::vector<KibitzerTournamentStats::HandResult> chipHands;
    std::vector<KibitzerTournamentStats::HandResult> deepHands, middleHands, shortHands;
    QVariantList recentSettlements;
    chipHands.reserve(static_cast<std::size_t>(m_kibitzerHistory.size()));
    for (const QVariant &v : m_kibitzerHistory) {
        const QVariantMap hand = v.toMap();
        const QVariantMap settled = hand.value("settlement").toMap();
        if (!settled.value("observed").toBool()) continue;
        KibitzerTournamentStats::HandResult item;
        item.valid = true;
        item.startingChips = settled.value("startingChips").toInt();
        item.endingChips = settled.value("endingChips").toInt();
        item.bigBlind = settled.value("bigBlind").toInt();
        if (!KibitzerTournamentStats::admissible(item)) continue;
        chipHands.push_back(item);
        QVariantMap line = settled;
        line["hand"] = hand.value("hand");
        line["session"] = hand.value("session");
        recentSettlements.prepend(line);
        if (recentSettlements.size() > 12) recentSettlements.removeLast();
        const double depth = double(item.startingChips) / item.bigBlind;
        if (depth > 40.0) deepHands.push_back(item);
        else if (depth >= 15.0) middleHands.push_back(item);
        else shortHands.push_back(item);
    }
    std::vector<KibitzerTournamentStats::Finish> finishRows;
    finishRows.reserve(static_cast<std::size_t>(m_kibitzerTournaments.size()));
    QVariantList recentFinishes;
    for (const QVariant &v : m_kibitzerTournaments) {
        const QVariantMap row = v.toMap();
        KibitzerTournamentStats::Finish f;
        f.entrants = row.value("entrants").toInt();
        f.place = row.value("place").toInt();
        f.completed = row.value("completed").toBool();
        finishRows.push_back(f);
    }
    for (int i = m_kibitzerTournaments.size() - 1;
         i >= 0 && recentFinishes.size() < 12; --i)
        recentFinishes.append(m_kibitzerTournaments.at(i));

    const auto perf = KibitzerTournamentStats::summarize(chipHands,finishRows);
    QVariantMap tournament;
    tournament["bbHands"] = perf.hands;
    tournament["netChips"] = static_cast<qlonglong>(perf.netChips);
    tournament["bbWon"] = perf.totalBb;
    tournament["bbPer100"] = perf.bbPer100;
    tournament["completed"] = perf.completedTournaments;
    tournament["wins"] = perf.wins;
    tournament["winPercent"] = KibitzerTrainingStats::percent(perf.wins,perf.completedTournaments);
    tournament["averagePlace"] = perf.averagePlace;
    tournament["averageNormalizedPlace"] = perf.averageNormalizedPlace;
    tournament["abandoned"] = perf.abandonedTournaments;
    tournament["deepHands"] = static_cast<int>(deepHands.size());
    tournament["deepBbPer100"] = KibitzerTournamentStats::summarize(deepHands,{}).bbPer100;
    tournament["mediumHands"] = static_cast<int>(middleHands.size());
    tournament["mediumBbPer100"] = KibitzerTournamentStats::summarize(middleHands,{}).bbPer100;
    tournament["shortHands"] = static_cast<int>(shortHands.size());
    tournament["shortBbPer100"] = KibitzerTournamentStats::summarize(shortHands,{}).bbPer100;
    tournament["recentFinishes"] = recentFinishes;
    tournament["recentSettlements"] = recentSettlements;
    tournament["historyLimit"] = kMaxTournaments;
    tournament["active"] = m_kibitzerTournamentActive;
    tournament["currentHands"] = m_kibitzerTournamentHandCount;
    tournament["currentEntrants"] = m_kibitzerTournamentEntrants;
    tournament["currentStack"] = m_kibitzerTournamentLastCash;
    tournament["currentBbWon"] = m_kibitzerTournamentBbWon;
    tournament["currentBbPer100"] = m_kibitzerTournamentHandCount > 0
        ? 100.0 * m_kibitzerTournamentBbWon / m_kibitzerTournamentHandCount : 0.0;
    result["tournament"] = tournament;

    result["disclaimer"] = QStringLiteral(
        "Tracked hands must contain a recorded human decision. Free-check folds are provable errors; "
        "review warnings use random or user-assigned opponent ranges and are NOT confirmed mistakes. "
        "Comparing model-warning rates across changing range assumptions does not measure skill. "
        "VPIP/PFR denominators are tracked hands with a preflop decision, excluding blind-only walks. "
        "Tournament BB/100 is realized chip movement divided by the BB on each hand, "
        "NOT all-in adjusted EV and NOT an ICM-adjusted measure of tournament value. "
        "Older hands before chip settlement recording are excluded from BB/100.");
    m_kibitzerProgress = result;
    emit kibitzerProgressChanged();
}

bool GameHandler::kibitzerCopyBackup() const {
    if (!m_kibitzerHistoryWritable || !QGuiApplication::clipboard()) return false;
    const QByteArray bytes=QJsonDocument(makeArchive(m_kibitzerHistory,m_kibitzerTournaments,m_kibitzerSessionId))
                           .toJson(QJsonDocument::Compact);
#ifdef Q_OS_ANDROID
    // The Android clipboard uses Binder; large JSON archives cannot be
    // transferred reliably.  Use the file-picker export instead.
    if (bytes.size() > 128 * 1024) return false;
#endif
    QGuiApplication::clipboard()->setText(QString::fromUtf8(bytes));
    return true;
}


// Export without going through Android's clipboard/Binder size limit.
// The JSON is first staged in app-private cache; a dedicated Android Activity
// receives the file picker's result and streams bytes into the selected document.
bool GameHandler::kibitzerExportBackup() const {
#ifdef Q_OS_ANDROID
    if (!m_kibitzerHistoryWritable) return false;
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty() || !QDir().mkpath(cacheDir)) return false;
    const QString stagedPath = cacheDir + QStringLiteral("/pokerth-kibitzer-export.json");

    // Stream ALL durable records into a compatible JSON archive. The rolling
    // in-memory history contains only the latest 2500 hands; it is never a
    // sufficient source for an extended overnight/month-long export.
    QSaveFile staged(stagedPath);
    if (!staged.open(QIODevice::WriteOnly)) return false;
    const QByteArray prefix = QByteArray("{\\"schemaVersion\\":1,\\"lastSessionId\\":")
        + QByteArray::number(m_kibitzerSessionId) + ",\\"hands\\":[";
    if (staged.write(prefix)!=prefix.size()) return false;
    const QString sourcePath=journalPath();
    if (QFile::exists(sourcePath)) {
        for (int pass=0;pass<2;++pass) {
            if (pass==1 && staged.write("],\\"tournaments\\":[")<0) return false;
            QFile journal(sourcePath);
            if (!journal.open(QIODevice::ReadOnly)) return false;
            bool first=true;
            while (!journal.atEnd()) {
                const QByteArray line=journal.readLine();
                if (!line.endsWith('\\n')) return false;
                QJsonParseError error;
                const QJsonDocument record=QJsonDocument::fromJson(line,&error);
                if (error.error!=QJsonParseError::NoError || !record.isObject()) return false;
                const QJsonObject item=record.object();
                const QString type=item.value(QStringLiteral("type")).toString();
                if (type != (pass==0?QLatin1String("hand"):QLatin1String("tournament")))
                    continue;
                if (!item.value(QStringLiteral("value")).isObject()) return false;
                if (!first && staged.write(",")!=1) return false;
                first=false;
                const QByteArray data=QJsonDocument(item.value(QStringLiteral("value")).toObject())
                                      .toJson(QJsonDocument::Compact);
                if (staged.write(data)!=data.size()) return false;
            }
        }
    } else {
        // Brand-new installation before any journal events.
        const QByteArray data=QJsonDocument(makeArchive(m_kibitzerHistory,
            m_kibitzerTournaments,m_kibitzerSessionId)).toJson(QJsonDocument::Compact);
        // The file may be absent only for a fresh, empty archive.
        if (!m_kibitzerHistory.isEmpty() || !m_kibitzerTournaments.isEmpty()) return false;
        Q_UNUSED(data);
        if (staged.write("],\\"tournaments\\":[")<0) return false;
    }
    if (staged.write("]}")!=2 || !staged.commit()) return false;

    auto openPicker = [stagedPath]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) {
            qWarning() << "[KIBITZER-EXPORT] Android activity unavailable";
            return;
        }
        QJniObject path = QJniObject::fromString(stagedPath);
        QJniObject::callStaticMethod<void>(
            "org/pokerth/qml/HistoryExportActivity", "start",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            activity.object(), path.object());
        QJniEnvironment env;
        if (env.checkAndClearExceptions())
            qWarning() << "[KIBITZER-EXPORT] Android file picker could not start";
    };
    if (auto *iface = qApp->nativeInterface<QNativeInterface::QAndroidApplication>())
        iface->runOnAndroidMainThread(openPicker);
    else
        openPicker();
    return true; // staged and picker requested; Android reports save result
#else
    return false;
#endif
}

bool GameHandler::kibitzerImportBackup(const QString &json) {
    const QByteArray bytes = json.toUtf8();
    if (!m_kibitzerHistoryWritable || bytes.size() > kMaxBytes) return false;
    QJsonParseError error;
    const QJsonDocument doc=QJsonDocument::fromJson(bytes,&error);
    if (error.error != QJsonParseError::NoError || !validateArchive(doc)) return false;
    // Import REPLACES the existing history, never merges or duplicates hands.
    // Caller must explicitly click the labelled Restore button in QML.
    const QJsonObject root=doc.object();
    const QVariantList previousHistory=m_kibitzerHistory;
    const QVariantList previousTournaments=m_kibitzerTournaments;
    const int previousSession=m_kibitzerSessionId;
    m_kibitzerHistory = root.value(QStringLiteral("hands")).toArray().toVariantList();
    m_kibitzerTournaments = root.value(QStringLiteral("tournaments")).toArray().toVariantList();
    int prev=root.value(QStringLiteral("lastSessionId")).toInt();
    for (const QVariant &v : m_kibitzerHistory)
        prev=std::max(prev,v.toMap().value("session").toInt());
    for (const QVariant &v : m_kibitzerTournaments)
        prev=std::max(prev,v.toMap().value("session").toInt());
    m_kibitzerSessionId=prev+1; // the import starts a fresh practice session
    saveKibitzerProgress();
    if (!m_kibitzerStorageError.isEmpty()) {
        // QSaveFile leaves the original archive intact; restore RAM as well.
        m_kibitzerHistory=previousHistory;
        m_kibitzerTournaments=previousTournaments;
        m_kibitzerSessionId=previousSession;
        rebuildKibitzerProgress();
        return false;
    }
    rebuildKibitzerProgress();
    return true;
}
