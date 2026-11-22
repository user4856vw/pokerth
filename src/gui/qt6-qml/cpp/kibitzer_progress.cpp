// PokerTH Kibitzer v3 — private, persistent training history.
// No network access and no reading of opponents' unrevealed cards.
#include "gamehandler.h"
#include "kibitzer_training_stats.h"
#include <QClipboard>
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
#include <QStandardPaths>
#include <algorithm>
#include <vector>

namespace {
constexpr int kSchema = 1;
constexpr int kMaxHands = 2500;
constexpr int kMaxBytes = 8 * 1024 * 1024;
QString statsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/kibitzer_training_v1.json");
}
QJsonObject makeArchive(const QVariantList &hands, int lastSession) {
    QJsonObject out;
    out[QStringLiteral("schemaVersion")] = kSchema;
    out[QStringLiteral("lastSessionId")] = lastSession;
    out[QStringLiteral("hands")] = QJsonArray::fromVariantList(hands);
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
    if (hands.size() > kMaxHands) return false;
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

void GameHandler::loadKibitzerProgress() {
    m_kibitzerHistory.clear();
    m_kibitzerSessionId = 1;
    m_kibitzerHistoryWritable = true;
    QFile f(statsPath());
    if (f.exists()) {
        if (!f.open(QIODevice::ReadOnly) || f.size() > kMaxBytes) {
            m_kibitzerHistoryWritable = false;
            m_kibitzerStorageError = QStringLiteral("Cannot read training archive; original protected.");
        } else {
            const QByteArray bytes = f.readAll();
            QJsonParseError parse;
            QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse);
            if (parse.error != QJsonParseError::NoError || !validateArchive(doc)) {
                m_kibitzerHistoryWritable = false;
                m_kibitzerStorageError = QStringLiteral("Training archive invalid; original protected.");
            } else {
                const QJsonObject root = doc.object();
                m_kibitzerHistory = root.value(QStringLiteral("hands")).toArray().toVariantList();
                int previous = root.value(QStringLiteral("lastSessionId")).toInt();
                for (const QVariant &v : m_kibitzerHistory)
                    previous = std::max(previous, v.toMap().value("session").toInt());
                m_kibitzerSessionId = std::max(1,previous+1);
            }
        }
    }
    rebuildKibitzerProgress();
}

void GameHandler::saveKibitzerProgress() {
    if (!m_kibitzerHistoryWritable) return;
    const QString path = statsPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        m_kibitzerStorageError = QStringLiteral("Cannot create training data directory.");
        return;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        m_kibitzerStorageError = QStringLiteral("Cannot open training data file for writing.");
        return;
    }
    const QByteArray bytes = QJsonDocument(makeArchive(m_kibitzerHistory, m_kibitzerSessionId))
                                  .toJson(QJsonDocument::Compact);
    if (bytes.size() > kMaxBytes || f.write(bytes) != bytes.size() || !f.commit()) {
        f.cancelWriting();
        m_kibitzerStorageError = QStringLiteral("Training archive not saved; check storage.");
        return;
    }
    m_kibitzerStorageError.clear();
}

void GameHandler::persistKibitzerHand() {
    if (!m_kibitzerHistoryWritable || m_kibitzerEvents.isEmpty()) return;
    QVariantMap hand = m_kibitzerLastReview;
    hand["session"] = m_kibitzerSessionId;
    hand["completedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    // No reference to hidden hands, network IDs, or online-game data.
    m_kibitzerHistory.append(hand);
    while (m_kibitzerHistory.size() > kMaxHands) m_kibitzerHistory.removeFirst();
    saveKibitzerProgress();
    rebuildKibitzerProgress();
}

void GameHandler::rebuildKibitzerProgress() {
    using KibitzerTrainingStats::Hand;
    const auto hands = convertHands(m_kibitzerHistory);
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
        item["fromDate"]=m_kibitzerHistory.at(static_cast<int>(start)).toMap().value("completedAt");
        item["toDate"]=m_kibitzerHistory.at(static_cast<int>(end-1)).toMap().value("completedAt");
        sessions.append(item);
        end=start;
    }
    result["sessions"] = sessions;
    result["disclaimer"] = QStringLiteral(
        "Tracked hands must contain a recorded human decision. Free-check folds are provable errors; "
        "review warnings use random or user-assigned opponent ranges and are NOT confirmed mistakes. "
        "Comparing model-warning rates across changing range assumptions does not measure skill. "
        "VPIP/PFR denominators are tracked hands with a preflop decision, excluding blind-only walks. "
        "Raw chip winnings are deliberately excluded: they mix skill and card luck.");
    m_kibitzerProgress = result;
    emit kibitzerProgressChanged();
}

bool GameHandler::kibitzerCopyBackup() const {
    if (!m_kibitzerHistoryWritable || !QGuiApplication::clipboard()) return false;
    const QByteArray bytes=QJsonDocument(makeArchive(m_kibitzerHistory,m_kibitzerSessionId))
                           .toJson(QJsonDocument::Compact);
    QGuiApplication::clipboard()->setText(QString::fromUtf8(bytes));
    return true;
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
    const int previousSession=m_kibitzerSessionId;
    m_kibitzerHistory = root.value(QStringLiteral("hands")).toArray().toVariantList();
    int prev=root.value(QStringLiteral("lastSessionId")).toInt();
    for (const QVariant &v : m_kibitzerHistory)
        prev=std::max(prev,v.toMap().value("session").toInt());
    m_kibitzerSessionId=prev+1; // the import starts a fresh practice session
    saveKibitzerProgress();
    if (!m_kibitzerStorageError.isEmpty()) {
        // QSaveFile leaves the original archive intact; restore RAM as well.
        m_kibitzerHistory=previousHistory;
        m_kibitzerSessionId=previousSession;
        rebuildKibitzerProgress();
        return false;
    }
    rebuildKibitzerProgress();
    return true;
}
