import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Local-only training adviser and review. Does not send poker actions.
Rectangle {
    id: kb
    // Force readable contrast regardless of the platform's Qt Controls style:
    // the stock Button control rendered black labels over near-black surfaces
    // under some Android themes.
    component TrainerButton: Button {
        id: trainerButton
        font.pixelSize: 12
        font.bold: true
        padding: 8
        contentItem: Text {
            text: trainerButton.text
            color: trainerButton.enabled ? "#f6f9ff" : "#a6b7ca"
            font: trainerButton.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            implicitWidth: 68
            implicitHeight: 34
            radius: 6
            border.width: 1
            border.color: trainerButton.activeFocus ? "#e2d089" : "#8299b4"
            color: !trainerButton.enabled ? "#293545"
                   : trainerButton.down ? "#55749a"
                   : trainerButton.hovered ? "#425f7e"
                   : "#304961"
        }
    }
    component TrainerRangeField: TextField {
        id: trainerRange
        color: "#f6f9ff"
        placeholderTextColor: "#b3c6df"
        selectionColor: "#4f79a3"
        selectedTextColor: "#ffffff"
        font.pixelSize: 12
        selectByMouse: true
        background: Rectangle {
            radius: 6
            color: "#293d54"
            border.color: trainerRange.activeFocus ? "#e2d089" : "#8299b4"
            border.width: 1
        }
    }
    color: "#eb19212d"
    radius: 10
    border.width: 1
    border.color: "#a2b3d0"
    property var advice: ({"status": "unavailable"})
    property bool expanded: false
    property bool openRequested: false
    readonly property bool editingRestore: restoreArea.activeFocus
    readonly property bool popupOpen: explanationPopup.visible || progressPopup.visible || rangePopup.visible || betPopup.visible
    property var rangePlayers: []
    property string rangeFeedback: ""
    property string betFeedback: ""
    readonly property bool customRangeActive: advice.rangeStatus === "ready"
    readonly property bool hasAggressionSignal: advice.status === "ready" && advice.aggression
                  && advice.aggression.status !== "none"
    function refreshOpponentRanges() {
        rangePlayers = GameTable ? GameTable.kibitzerOpponentRanges : []
    }
    function applyRange(seat, notation) {
        if (!GameTable) return
        var r = GameTable.kibitzerSetOpponentRange(seat, notation)
        rangeFeedback = (r.ok ? "✓ " : "⚠ ") + (r.message || "")
        if (r.ok) {
            refreshOpponentRanges()
            if (myTurnNow) debounce.restart()
        }
    }
    function activeBetSeat() {
        var r = advice.activeRanges || []
        return r.length === 1 ? Number(r[0].seatId) : -1
    }
    function activeResponse() {
        var seat = activeBetSeat()
        for (var i=0;i<rangePlayers.length;++i)
            if (Number(rangePlayers[i].seatId) === seat) return rangePlayers[i]
        return {}
    }
    function openBetPopup() {
        refreshOpponentRanges()
        betFeedback = ""
        betPopup.open()
    }
    function saveBetResponse() {
        if (!GameTable) return
        var seat = activeBetSeat()
        if (seat < 1) {
            betFeedback = "⚠ Only one opponent can be modeled at a time."
            return
        }
        var result = GameTable.kibitzerSetBetResponse(seat,
            halfContinueField.text, fullContinueField.text)
        betFeedback = (result.ok ? "✓ " : "⚠ ") + (result.message || "")
        if (result.ok) {
            refreshOpponentRanges()
            debounce.restart()
        }
    }
    function betEvSummary() {
        var m = advice.betSizing || {}
        if (m.status === "ready") {
            var r = m.best || {}
            if (m.modeledRaiseEdge)
                return "BET EV: " + r.label + " · spend " + r.size
                    + " · advantage +" + Number(r.delta || 0).toFixed(1)
                    + " chips ⓘ"
            return "BET EV: no robust modeled raise edge · compare sizes ⓘ"
        }
        if (m.status === "unconfigured")
            return "BET EV: configure opponent responses ⓘ"
        return "BET EV: " + (m.message || "no modeled size") + " ⓘ"
    }
    function describeRangeScenario() {
        var a = advice
        if (a.status !== "ready") {
            explain("Range sensitivity", "No live equity calculation is available.")
            return
        }
        var body = "Baseline against uniformly random hands: "
            + fmt(100*a.equity) + ".\n"
        if (a.rangeStatus === "baseline") {
            body += "No active opponent has a manual range. Use RANGES to "
                + "set a hypothesis for each seat; leave blank for random.\n"
        } else if (a.rangeStatus === "ready") {
            var b = a.rangeScenario
            body += "Conditioned equity under your assumptions: "
                 + fmt(100*a.rangeEquity) + "; change from random: "
                 + (a.rangeDelta >= 0 ? "+" : "") + (100*a.rangeDelta).toFixed(1)
                 + " percentage points.\n"
            body += "Monte Carlo samples: " + b.samples + "/" + b.samplesRequested
                 + "; seed: " + b.seed
                 + "; sampling standard error: "
                 + (100*a.rangeStdError).toFixed(2) + " percentage points.\n"
            body += "Call threshold: " + fmt(100*a.required) + ". "
                 + "Advice comparison margin: " + (100*a.comparisonMargin).toFixed(2)
                 + " percentage points (" + a.comparisonMarginBasis + ").\n"
            body += "Your range hypothesis is NOT statistically validated by "
                 + "these error bars.\n"
        } else {
            body += "Range scenario unavailable: " + (a.rangeMessage || "insufficient compatible deals") + "\n"
        }
        var rows = a.activeRanges || []
        body += "\nActive opponents and assumptions:\n"
        for (var i = 0; i < rows.length; ++i)
            body += rows[i].name + " (seat " + rows[i].seatId + "): "
                  + (rows[i].notation || "ANY / random") + "\n"
        body += "\nRanges cover starting-hand TYPES, not fixed cards. "
            + "For instance, AA represents six combinations before blockers; AKs four. "
            + "Opponent hands are sampled independently within each range, "
            + "then the entire deal is rejected on overlap. Future board cards "
            + "are drawn from the remaining deck. No hidden bot cards are read. "
            + "The spread between the two estimates is model sensitivity, NOT "
            + "a confidence interval. Opponent reaction to betting, fold equity, "
            + "future bets, side pots, and tournament ICM remain unmodeled."
        explain("Range sensitivity — inputs and methodology", body)
    }
    signal closeRequested()
    property string explanationTitle: ""
    property string explanationBody: ""
    property string replayResult: ""
    property var replayEvent: null
    property var recentDecisions: []
    readonly property bool localEligible: GameTable && !GameTable.spectating
                                          && GameTable.isLocalGameRunning()
    property bool forceReview: false
    property string backupMessage: ""
    property bool confirmRestore: false
    readonly property bool myTurnNow: GameTable && (GameTable.myTurn || GameTable.awaitingMyAction)
    readonly property var report: GameTable ? GameTable.kibitzerReview : ({"entries": []})
    readonly property var progress: GameTable ? GameTable.kibitzerProgress : ({"totalHands": 0})
    readonly property bool hasReview: report && report.entries && report.entries.length > 0
    readonly property bool reviewMode: forceReview || !myTurnNow
    implicitHeight: (expanded && reviewMode) ? Math.min(288, 95 + reviewLines.implicitHeight)
                    : (expanded ? 190 + (hasAggressionSignal ? 25 : 0)
                      : (myTurnNow && advice.rangeStatus !== "baseline" ? 125 : 100)
                        + (hasAggressionSignal ? 25 : 0))
    visible: openRequested && localEligible
    function fmt(n) { return Number(n || 0).toFixed(1) + "%" }
    function explain(title, body, decision) {
        if (progressPopup.visible) progressPopup.close()
        explanationTitle = title
        explanationBody = body
        replayResult = ""
        replayEvent = decision || null
        explanationPopup.open()
    }
    function rate(label, numerator, denominator) {
        var n = Number(numerator || 0), d = Number(denominator || 0)
        return label + ": " + n + "/" + d + " × 100 = "
               + (d ? kb.fmt(100 * n / d) : "N/A (no observations)") + "\n"
    }
    function signedNumber(v, digits) {
        var x = Number(v || 0)
        return (x > 0 ? "+" : "") + x.toFixed(digits || 1)
    }
    function explainTournamentRecord(rec) {
        var t = rec || {}
        var text = "Tournament session: " + (t.session || "?") + ".\n"
            + "Initial entrants: " + (t.entrants || "?") + ".\n"
            + (t.completed
                 ? "Finishing place: #" + t.place + "/" + t.entrants
                    + (t.tiedPlace ? " (simultaneous elimination / shared rank)" : "") + ".\n"
                 : "Abandoned: no finishing rank is assigned.\n")
            + "Settled hands while hero remained in the tournament: " + t.hands + ".\n"
            + "Sum of blind-normalized chip changes: " + kb.signedNumber(t.bbWon,2) + " BB.\n"
            + "Per-tournament realized chip BB/100: "
            + (t.hands ? kb.signedNumber(100 * t.bbWon / t.hands,2) : "N/A")
            + " = 100 × " + kb.signedNumber(t.bbWon,2) + " / " + t.hands + ".\n"
            + "Stack at end: " + t.lastStack + " chips.\n"
            + "Recorded: " + t.finishedAt + ".\n\n"
            + "Finishing place is recorded at elimination or victory, not at an arbitrary "
            + "later hand. Simultaneous eliminations may share a rank. This measures "
            + "local tournament results only; it is not a cash-game profitability measure."
        kb.explain("Individual tournament — recorded outcome", text)
    }
    function explainTournament(topic) {
        var t = kb.progress.tournament || {}
        var body = "LOCAL TOURNAMENTS (completed games only)\n"
                 + "Completed " + (t.completed || 0) + "; wins " + (t.wins || 0)
                 + "; abandoned " + (t.abandoned || 0) + ".\n"
                 + "Win rate = wins / completed tournaments = "
                 + t.wins + "/" + t.completed + " × 100 = "
                 + (t.completed ? kb.fmt(t.winPercent) : "N/A") + ".\n"
                 + "Average place = sum of finishing ranks / completed tournaments: "
                 + (t.completed ? Number(t.averagePlace).toFixed(2) : "N/A") + ".\n"
                 + "A simultaneous elimination may share a place; aborted tournaments "
                 + "never contribute a made-up rank.\n\n"
                 + "REALIZED TOURNAMENT CHIP BB/100\n"
                 + "For every settled local hand, net chips = closing stack minus "
                 + "opening stack (before blinds). BB units = net chips / big blind "
                 + "ON THAT HAND.\n"
                 + "Sum of those BB units: " + kb.signedNumber(t.bbWon,2)
                 + " across " + t.bbHands + " eligible dealt hands.\n"
                 + "BB/100 = 100 × " + kb.signedNumber(t.bbWon,2) + " / " + t.bbHands
                 + " = " + (t.bbHands ? kb.signedNumber(t.bbPer100,2) : "N/A") + ".\n"
                 + "This includes blinds, antes, walks, folds and side-pot settlements, "
                 + "including hands with NO human decision. Older hands without "
                 + "settlement evidence are excluded. The archive retains at most "
                 + kb.progress.historyLimit + " settled/action hands.\n"
                 + "BB/100 is NOT all-in-adjusted expected value, a solver score, "
                 + "or an Independent Chip Model (ICM) measure of prize value.\n\n"
                 + "STACK DEPTH BREAKDOWN (before blinds)\n"
                 + ">40BB: " + t.deepHands + " hands, "
                 + (t.deepHands ? kb.signedNumber(t.deepBbPer100,1) : "N/A") + " BB/100.\n"
                 + "15–40BB: " + t.mediumHands + " hands, "
                 + (t.mediumHands ? kb.signedNumber(t.mediumBbPer100,1) : "N/A") + " BB/100.\n"
                 + "<15BB: " + t.shortHands + " hands, "
                 + (t.shortHands ? kb.signedNumber(t.shortBbPer100,1) : "N/A") + " BB/100.\n\n"
                 + "RECENT INDIVIDUAL HAND CONTRIBUTIONS\n"
        var rec = t.recentSettlements || []
        for (var i = 0; i < rec.length; ++i) {
            var h = rec[i]
            body += "Hand " + h.hand + ": (" + h.endingChips + " - "
                + h.startingChips + ") / " + h.bigBlind
                + " = " + kb.signedNumber(h.bbWon,2) + " BB\n"
        }
        if (!rec.length) body += "No new-style settled hands yet.\n"
        kb.explain(topic, body)
    }
    function explainSummary(title, m, context) {
        if (!m) return
        var body = (context || "") + "\n"
        body += "Hands: " + m.hands + "; recorded decisions: " + m.decisions + ".\n"
        body += kb.rate("Free-check folds", m.clearMistakes, m.freeOpportunities)
        body += kb.rate("Model review warnings", m.reviewWarnings, m.pricedReviewed)
        body += kb.rate("VPIP", m.vpipHands, m.preflopObservedHands)
        body += kb.rate("PFR", m.pfrHands, m.preflopObservedHands)
        body += "\nAction counts: folds " + m.folds + ", checks " + m.checks
             + ", calls " + m.calls + ", raises " + m.raises
             + ", all-ins " + m.allIns + ", ungraded " + m.ungraded + ".\n"
        body += "\nVPIP counts hands with voluntary preflop investment (not forced blinds); "
             + "PFR counts preflop raises. Their denominator is hands with at least one "
             + "recorded preflop decision. A review warning is not proof of a bad action. "
             + "A zero denominator is shown as 0.0% in the dashboard, not as evidence of skill."
        kb.explain(title, body)
    }
    function cardName(code) {
        var ranks = ["2","3","4","5","6","7","8","9","10","J","Q","K","A"]
        var suits = ["♦","♥","♠","♣"]
        return code >= 0 && code < 52 ? ranks[code % 13] + suits[Math.floor(code / 13)] : "?"
    }
    function cards(codes) {
        var out = []
        for (var i = 0; i < (codes || []).length; ++i) out.push(kb.cardName(codes[i]))
        return out.join(" ") || "none"
    }
    function explainEquity() {
        var a = kb.advice
        if (a.status !== "ready") {
            kb.explain("Equity estimate", "No live equity calculation is available at this moment.")
            return
        }
        var se = Number(a.equityStdError || 0)
        var body = "Hero: " + a.hero + "; board: " + kb.cards(a.boardCodes)
                 + "; active opponents: " + a.opponents + ".\n"
        body += "Method: " + a.modelVersion + ". Opponents' unseen cards are "
             + "uniformly random; future community cards are randomly dealt "
             + "without replacement. Opponents' actual hole cards are never inspected.\n"
        body += "Valid Monte Carlo samples: " + a.samples + "/" + a.samplesRequested
             + "; seed: " + a.seed + ".\n"
        body += "Each trial awards 1 for an outright win, 0 for a loss, and "
             + "1/(number of tied winners) for a split pot. Equity is the "
             + "average share of the pot: " + kb.fmt(100 * a.equity) + ".\n"
        body += "Outright-win frequency: " + kb.fmt(100 * a.win)
             + "; tie frequency: " + kb.fmt(100 * a.tie) + ".\n"
        body += "Estimated Monte Carlo standard error: " + kb.fmt(100 * se)
             + " percentage points; approximate 95% sampling margin ±"
             + kb.fmt(100 * 1.96 * se) + " points. This ignores opponent-range uncertainty.\n"
        if (a.rangeStatus !== "baseline")
            body += "\nA separate estimate uses your manual opponent ranges. Tap its "
                 + "value to inspect each range and the sampler. "
                 + "The baseline remains available for comparison.\n"
        body += "Showdown equity is not full expected value. Future betting, fold equity, "
             + "side pots and tournament ICM are excluded. The random baseline uses a "
             + "heuristic 12-percentage-point caution gap. When you assign ranges, "
             + "the pot-odds comparison instead uses an approximate 95% Monte Carlo "
             + "sampling margin, conditional on your assumptions being right. "
             + "That margin does NOT measure model or range-selection uncertainty."
        kb.explain("Equity — method and uncertainty", body)
    }
    function explainAggression() {
        var a = kb.advice
        if (a.status !== "ready" || !a.aggression || a.aggression.status === "none")
            return
        var g = a.aggression
        var body = "Stage A — a candidate to evaluate, NOT an instruction to bet.\n\n"
                 + g.reason + "\n\n"
                 + "Your hole cards: " + a.hero + "; board: " + kb.cards(a.boardCodes)
                 + "; opponents still in hand: " + a.opponents + ".\n"
                 + "Conditional equity: " + kb.fmt(100*g.equity)
                 + "; cautious comparison estimate: " + kb.fmt(100*g.lowerEquity) + ".\n"
                 + "Assumption: " + g.conditionalOn + ".\n"
                 + "Pot: " + g.pot + " chips; call price: " + g.price
                 + "; legal raise control spans " + g.minRaise + "–"
                 + g.maxRaise + " chips. These are legal limits, NOT suggested sizes.\n"
                 + "Passive comparison: " + a.passiveHeadline + ".\n"
                 + "Why this is not an EV comparison: we have NOT estimated which "
                 + "weaker hands call, which better hands fold, how often they reraise, "
                 + "or what happens on later streets. A premium preflop hand or a "
                 + "strong draw is only a screening criterion.\n\n"
                 + g.explanation
        kb.explain("Aggression signal — assumptions and limitations", body)
    }
    function explainThreshold() {
        var a = kb.advice
        if (a.status !== "ready") return
        var p = Number(a.pot || 0), c = Number(a.price || 0)
        var body = "Current pot including committed bets: " + p
             + " chips. Additional amount to call: " + c + " chips.\n"
             + "Break-even immediate call equity = call amount / "
             + "(current pot + call amount).\n"
             + "Here: " + c + " / (" + p + " + " + c + ") = "
             + kb.fmt(100 * a.required) + ".\n"
             + "A free check costs 0, so its threshold is 0%. This simple "
             + "model assumes no further betting and no changes in opponent ranges, "
             + "side-pot eligibility or ICM. It does not prove calling is optimal."
        kb.explain("Call threshold — formula and inputs", body)
    }
    function explainDecision(e) {
        var a = e.snapshot || {}
        var body = "Session " + (e.session || "?") + ", hand " + (e.hand || "?")
             + ", street " + e.phase + ". You chose " + e.action
             + (e.amount ? " (" + e.amount + " chips)" : "") + ".\n"
             + "Review: " + e.rating + " — " + e.explanation + ".\n"
             + "Pot " + e.pot + ", call " + e.price + ", position " + e.position + ".\n"
        if (a.status === "ready" && a.modelVersion) {
            if (a.rangeScenario && a.rangeScenario.modelVersion) {
                var rs = a.rangeScenario
                body += "\nRANGE SENSITIVITY AT DECISION TIME:\n"
                body += "Uniform random equity: " + kb.fmt(100 * a.equity) + ".\n"
                body += "Manual range equity: " + kb.fmt(100 * rs.equity)
                     + " (" + rs.samples + "/" + rs.samplesRequested + " samples).\n"
                body += "Assumptions saved with seed " + rs.seed + ":\n"
                for (var k = 0; k < rs.ranges.length; ++k)
                    body += "  " + rs.ranges[k].name + ": "
                         + (rs.ranges[k].notation || "ANY / random") + "\n"
                body += "Those saved ranges, not the current editor settings, "
                     + "are used during replay.\n"
            }
            body += "\nSnapshot: hero " + kb.cards(a.heroCodes)
                 + "; board " + kb.cards(a.boardCodes)
                 + "; opponents " + a.opponents + "; phase " + a.phase + ".\n"
                 + "Model " + a.modelVersion + "; RNG seed " + a.seed
                 + "; samples " + a.samples + "/" + a.samplesRequested + ".\n"
                 + "Saved equity " + kb.fmt(100 * a.equity)
                 + " ± " + kb.fmt(100 * 1.96 * Number(a.equityStdError || 0))
                 + " percentage points (approx. 95% Monte Carlo margin); call threshold "
                 + kb.fmt(100 * a.required) + ".\n"
                 + "Press REPLAY CALCULATION to verify the stored estimate using the same random stream."
            kb.explain("Saved decision — evidence and replay", body, e)
        } else {
            body += "\nNo seeded snapshot is available for this decision. "
                 + "Older decisions cannot be reconstructed from aggregate statistics."
            kb.explain("Recorded decision", body)
        }
    }
    function doReplay() {
        if (!replayEvent || !GameTable) return
        var r = GameTable.kibitzerReplayDecision(replayEvent)
        replayResult = r.status === "ready"
            ? "Random baseline: " + kb.fmt(100 * r.equity)
              + " vs saved " + kb.fmt(100 * r.originalEquity)
              + "; samples " + r.samples
              + "; deterministic replay: " + (r.matched ? "MATCH" : "MISMATCH")
            : "Cannot replay: " + (r.reason || "unknown reason")
        if (r.rangeStatus === "ready" || r.rangeStatus === "insufficient")
            replayResult += "\nManual range replay: " + kb.fmt(100 * r.rangeEquity)
                 + " vs saved " + kb.fmt(100 * r.rangeOriginalEquity)
                 + "; compatible samples " + r.rangeSamples
                 + "; replay: " + (r.rangeMatched ? "MATCH" : "MISMATCH")
                 + (r.rangeStatus === "insufficient" ? " (insufficient sample size)" : "")
        else if (r.rangeStatus === "invalid")
            replayResult += "\nRange replay: " + r.rangeMessage
    }
    function refreshHistory() {
        recentDecisions = GameTable ? GameTable.kibitzerRecentDecisions(20) : []
    }

    function refreshAdvice() {
        if (visible && myTurnNow && GameTable)
            advice = GameTable.kibitzerAdvice()
    }
    Timer { id: debounce; interval: 180; onTriggered: kb.refreshAdvice() }
    onMyTurnNowChanged: {
        forceReview = false
        if (myTurnNow) debounce.restart()
    }
    onVisibleChanged: {
        if (visible && myTurnNow) debounce.restart()
        if (visible) {
            kb.refreshHistory()
            kb.refreshOpponentRanges()
        }
    }
    Connections {
        target: GameTable
        function onPlayersChanged() { if (kb.myTurnNow) debounce.restart() }
        function onBoardCardsChanged() { if (kb.myTurnNow) debounce.restart() }
        function onCallAmountChanged() { if (kb.myTurnNow) debounce.restart() }
        function onTotalPotChanged() { if (kb.myTurnNow) debounce.restart() }
        function onKibitzerProgressChanged() { kb.refreshHistory() }
        function onKibitzerRangesChanged() {
            kb.refreshOpponentRanges()
            if (kb.visible && kb.myTurnNow) debounce.restart()
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4
        Row {
            width: parent.width
            spacing: 7
            Text {
                width: parent.width - 225
                color: "#ebd68a"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
                text: kb.reviewMode && kb.hasReview
                      ? "HAND #" + kb.report.hand + " · " + kb.report.clearMistakes
                          + " certain · " + kb.report.reviewWarnings + " review"
                      : "KIBITZER · " + (kb.advice.status === "ready"
                         ? kb.advice.hero + " · " + kb.advice.phase
                         : "training")
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        kb.expanded = !kb.expanded
                        if (kb.myTurnNow && kb.hasReview) kb.forceReview = !kb.forceReview
                    }
                }
            }
            Rectangle {
                width: 78; height: 23
                radius: 5; color: "#465b6f"; border.color: "#9eb7cc"
                Text {
                    anchors.centerIn: parent
                    text: "RANGES"; color: "white"; font.pixelSize: 11; font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        kb.refreshOpponentRanges()
                        kb.rangeFeedback = ""
                        rangePopup.open()
                    }
                }
            }
            Rectangle {
                width: 88
                height: 23
                radius: 5
                color: "#435675"
                border.color: "#93a9d4"
                Text {
                    anchors.centerIn: parent
                    text: "PROGRESS"
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        kb.refreshHistory()
                        progressPopup.open()
                    }
                }
            }
            Rectangle {
                width: 28; height: 23
                radius: 5; color: "#5a3e49"; border.color: "#b8a0ab"
                Text { anchors.centerIn: parent; text: "×"; color: "white"; font.pixelSize: 18 }
                MouseArea { anchors.fill: parent; onClicked: kb.closeRequested() }
            }
        }
        Text {
            visible: !kb.reviewMode
            width: parent.width
            color: "white"
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideRight
            text: kb.advice.status === "ready" ? kb.advice.headline : "No advice available"
        }
        Row {
            visible: !kb.reviewMode && kb.advice.status === "ready"
            width: parent.width
            spacing: 8
            Text {
                width: (parent.width - 8) / 2
                color: "#d9e5f6"; font.pixelSize: 11; elide: Text.ElideRight
                text: "Random: " + kb.fmt(100*kb.advice.equity) + " ⓘ"
                MouseArea { anchors.fill: parent; onClicked: kb.explainEquity() }
            }
            Text {
                width: (parent.width - 8) / 2
                color: "#d9e5f6"; font.pixelSize: 11; elide: Text.ElideRight
                text: "Call threshold: " + kb.fmt(100*kb.advice.required) + " ⓘ"
                MouseArea { anchors.fill: parent; onClicked: kb.explainThreshold() }
            }
        }
        Row {
            visible: !kb.reviewMode && kb.advice.status === "ready"
                     && kb.advice.rangeStatus !== "baseline"
            width: parent.width
            spacing: 8
            Text {
                width: (parent.width - 8) / 2
                color: kb.customRangeActive ? "#b5f0d2" : "#ffb4a3"
                font.pixelSize: 11; elide: Text.ElideRight
                text: kb.customRangeActive
                    ? "Your ranges: " + kb.fmt(100 * kb.advice.rangeEquity) + " ⓘ"
                    : "Ranges: not reliable ⓘ"
                MouseArea { anchors.fill: parent; onClicked: kb.describeRangeScenario() }
            }
            Text {
                width: (parent.width - 8) / 2
                color: "#d9e5f6"; font.pixelSize: 11; elide: Text.ElideRight
                text: kb.customRangeActive
                    ? "Δ vs random: " + (kb.advice.rangeDelta >= 0 ? "+" : "")
                       + (100 * kb.advice.rangeDelta).toFixed(1) + "pp ⓘ"
                    : "Edit ranges to retry"
                MouseArea { anchors.fill: parent; onClicked: kb.describeRangeScenario() }
            }
        }
        Text {
            visible: !kb.reviewMode && kb.advice.status === "ready"
            width: parent.width
            color: (kb.advice.betSizing && kb.advice.betSizing.modeledRaiseEdge)
                    ? "#b9f0d1" : "#f0d891"
            font.pixelSize: 11
            font.bold: true
            elide: Text.ElideRight
            text: kb.betEvSummary()
            MouseArea { anchors.fill: parent; onClicked: kb.openBetPopup() }
        }
        Text {
            visible: !kb.reviewMode && kb.hasAggressionSignal
            width: parent.width
            color: "#b9f0d1"
            font.pixelSize: 11
            font.bold: true
            elide: Text.ElideRight
            text: "Why consider aggression?  ⓘ  ·  " + kb.advice.passiveHeadline
            MouseArea { anchors.fill: parent; onClicked: kb.explainAggression() }
        }
        Text {
            visible: kb.expanded && !kb.reviewMode && kb.advice.status === "ready"
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#d9e5f6"
            font.pixelSize: 11
            text: kb.advice.explanation + " (Stage A ignores fold equity and tournament ICM; BET EV models conditional fold equity.)"
        }
        Text {
            visible: kb.reviewMode && !kb.expanded
            width: parent.width
            font.pixelSize: 12
            color: "white"
            text: kb.hasReview ? kb.report.entries.length + " decisions saved · tap to expand"
                : (kb.progress.totalHands || 0) + " hands tracked · tap PROGRESS"
        }
        Flickable {
            visible: kb.reviewMode && kb.expanded && kb.hasReview
            width: parent.width
            height: Math.min(215, reviewLines.implicitHeight)
            contentHeight: reviewLines.implicitHeight
            clip: true
            Column {
                id: reviewLines
                width: parent.width
                spacing: 7
                Repeater {
                    model: kb.hasReview ? kb.report.entries : []
                    delegate: Text {
                        required property var modelData
                        width: reviewLines.width
                        color: modelData.rating === "CLEAR MISTAKE" ? "#ff9c9c" :
                               modelData.rating === "REVIEW" ? "#ffe09b" : "#d9e5f6"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: "[" + modelData.rating + "] " + modelData.phase + " "
                              + modelData.action + ": " + modelData.explanation + " ⓘ"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainDecision(modelData)
                        }
                    }
                }
            }
        }
        Text {
            visible: kb.reviewMode && kb.expanded && kb.hasReview
            width: parent.width
            color: "#9fb0ca"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            text: "Only free-check folds are conclusive errors; other flags are hypothetical."
        }
    }

    Popup {
        id: betPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.max(8, (parent.width - width) / 2)
        y: Math.max(8, (parent.height - height) / 2)
        width: Math.min(560, parent.width - 16)
        height: Math.min(710, parent.height - 16)
        padding: 12
        onOpened: {
            kb.refreshOpponentRanges()
            var r = kb.activeResponse()
            halfContinueField.text = r.halfContinue || ""
            fullContinueField.text = r.potContinue || ""
        }
        background: Rectangle {
            radius: 13
            color: "#1c2636"
            border.color: "#94a9ca"
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    color: "#f0d891"
                    font.bold: true; font.pixelSize: 16
                    wrapMode: Text.WordWrap
                    text: "BET EV · STAGE B"
                }
                TrainerButton { text: "Close"; onClicked: betPopup.close() }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                Column {
                    width: parent.width
                    spacing: 10
                    Text {
                        width: parent.width; color: "#e1ecf9"
                        font.pixelSize: 12; wrapMode: Text.WordWrap
                        text: {
                            var p = kb.activeResponse()
                            return "Opponent: " + (p.name || "not available")
                                + " · Initial range: " + (p.notation || "ANY")
                                + ". Only heads-up, non-all-in local-game decisions are evaluated."
                        }
                    }
                    Text {
                        width: parent.width; color: "#b9d2ea"
                        font.pixelSize: 11; wrapMode: Text.WordWrap
                        text: "For each size, which starting hands would this opponent "
                             + "CALL with? All other hands in the INITIAL RANGES "
                             + "hypothesis are assumed to fold. These are YOUR response "
                             + "hypotheses, not the bot's known strategy. Blank disables a size."
                    }
                    Text {
                        width: parent.width; color: "#f0d891"
                        font.pixelSize: 12; font.bold: true
                        text: "Call range after +½ pot beyond a call"
                    }
                    TrainerRangeField {
                        id: halfContinueField
                        width: parent.width
                        placeholderText: "Example: 66+,ATs+,AJo+,KTs+,KQo,QJs,JTs"
                    }
                    Text {
                        width: parent.width; color: "#f0d891"
                        font.pixelSize: 12; font.bold: true
                        text: "Call range after +1 pot beyond a call"
                    }
                    TrainerRangeField {
                        id: fullContinueField
                        width: parent.width
                        placeholderText: "Example: 99+,AJs+,AQo+,KQs"
                    }
                    Row {
                        width: parent.width
                        spacing: 6
                        TrainerButton {
                            width: (parent.width - 12) / 3
                            text: "Save"
                            onClicked: kb.saveBetResponse()
                        }
                        TrainerButton {
                            width: (parent.width - 12) / 3
                            text: "Example vs ANY"
                            onClicked: {
                                halfContinueField.text =
                                    "22+,A2s+,A8o+,K8s+,KTo+,Q9s+,QJo,J9s+,T9s,98s,87s"
                                fullContinueField.text =
                                    "66+,ATs+,AJo+,KTs+,KQo,QJs,JTs"
                                kb.betFeedback = "Illustrative calling ranges only. Tap Save; "
                                    + "they must be subsets of your initial range."
                            }
                        }
                        TrainerButton {
                            width: (parent.width - 12) / 3
                            text: "Clear"
                            onClicked: {
                                halfContinueField.text = ""
                                fullContinueField.text = ""
                                kb.saveBetResponse()
                            }
                        }
                    }
                    Text {
                        visible: kb.betFeedback !== ""
                        width: parent.width
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        color: kb.betFeedback.indexOf("⚠") === 0
                            ? "#ffb3b3" : "#b9f0d1"
                        text: kb.betFeedback
                    }
                    Rectangle {
                        width: parent.width; height: 1; color: "#566a84"
                    }
                    Text {
                        width: parent.width; color: "#f0d891"
                        font.pixelSize: 13; font.bold: true; wrapMode: Text.WordWrap
                        text: "EXPECTED CHIP VALUE · terminal showdown"
                    }
                    Text {
                        width: parent.width; color: "#dce8f8"
                        font.pixelSize: 12; wrapMode: Text.WordWrap
                        text: {
                            var m = kb.advice.betSizing || {}
                            if (m.status !== "ready")
                                return m.message || "No EV estimate is available."
                            return "Reference: " + m.baselineAction
                                + " = " + kb.signedNumber(m.referenceEV,1) + " chips EV. "
                                + "P = " + m.pot + ", C = " + m.price
                                + ". Baseline calls/checks proceed directly to showdown."
                        }
                    }
                    Repeater {
                        model: (kb.advice.betSizing || {}).rows || []
                        delegate: Column {
                            required property var modelData
                            width: parent.width
                            spacing: 4
                            Text {
                                width: parent.width
                                color: modelData.status === "ready" ? "#b9f0d1" : "#e2c18c"
                                font.pixelSize: 12; font.bold: true; wrapMode: Text.WordWrap
                                text: modelData.label + " · spend " + modelData.size
                                    + " chips · " + modelData.status
                            }
                            Text {
                                width: parent.width; color: "#dce8f8"
                                font.pixelSize: 11; wrapMode: Text.WordWrap
                                text: {
                                    if (modelData.status !== "ready")
                                        return modelData.reason || "Not calculated."
                                    return "Call " + modelData.callCombos + "/"
                                        + modelData.priorCombos + " combinations; fold "
                                        + kb.fmt(100 * modelData.foldProbability)
                                        + "; equity if called "
                                        + kb.fmt(100 * modelData.callEquity)
                                        + "; opponent adds "
                                        + modelData.opponentAdditionalCall + " chips.\n"
                                        + "EV: " + kb.signedNumber(modelData.raiseEV,1)
                                        + " chips; Δ vs " + kb.advice.betSizing.baselineAction
                                        + " = " + kb.signedNumber(modelData.delta,1)
                                        + " chips; 95% sampling margin ±"
                                        + Number(modelData.samplingMargin).toFixed(1)
                                        + " chips. " + modelData.comparison
                                }
                            }
                            Rectangle {
                                width: parent.width; height: 1; color: "#566a84"
                            }
                        }
                    }
                    Text {
                        width: parent.width; color: "#b9f0d1"
                        font.pixelSize: 12; wrapMode: Text.WordWrap
                        text: {
                            var m = kb.advice.betSizing || {}
                            if (m.status !== "ready") return ""
                            if (!m.modeledRaiseEdge)
                                return "No clear raise advantage from these two sizes "
                                    + "after Monte Carlo sampling uncertainty."
                            return "MODELED BET/RAISE CANDIDATE: "
                                + m.best.label + " · spend " + m.best.size
                                + " chips · modeled advantage "
                                + kb.signedNumber(m.best.delta,1)
                                + " chips. Not an optimal-size claim."
                        }
                    }
                    Text {
                        width: parent.width; color: "#b8c9df"
                        font.pixelSize: 11; wrapMode: Text.WordWrap
                        text: "P = pot before acting, C = call cost, X = hero's new chips "
                            + "spent when raising. Opponent calls X-C more. "
                            + "EV(raise X) = f×P + (1-f)×[e_call×(P+2X-C) - X]. "
                            + "Reference EV = e_prior×P for CHECK, "
                            + "or max(0, e_prior×(P+C)-C) for FOLD/CALL. "
                            + "Only two exact sizes are considered, never an interpolated optimum. "
                            + "The caller range is chosen by hole-card type, not the board texture. "
                            + "Re-raises, future bets, ICM, model error and opponent adaptation "
                            + "are NOT modeled. No poker action is placed automatically."
                    }
                }
            }
        }
    }

    Popup {
        id: rangePopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.max(8, (parent.width - width) / 2)
        y: Math.max(8, (parent.height - height) / 2)
        width: Math.min(560, parent.width - 16)
        height: Math.min(690, parent.height - 16)
        padding: 12
        onOpened: kb.refreshOpponentRanges()
        background: Rectangle { radius: 13; color: "#1c2636"; border.color: "#94a9ca" }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true; color: "#f0d891"
                    text: "YOUR OPPONENT-RANGE ASSUMPTIONS"
                    font.bold: true; font.pixelSize: 14; wrapMode: Text.WordWrap
                }
                TrainerButton { text: "Close"; onClicked: rangePopup.close() }
            }
            Text {
                Layout.fillWidth: true
                text: "Per seat; used across hands of this local game. "
                      + "Blank / ANY means random. Edit each opponent independently. "
                      + "No hidden bot cards are consulted."
                color: "#d6e1f3"; font.pixelSize: 12; wrapMode: Text.WordWrap
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                Column {
                    width: parent.width
                    spacing: 12
                    Repeater {
                        model: kb.rangePlayers
                        delegate: Column {
                            id: opponentRow
                            required property var modelData
                            width: parent.width
                            spacing: 5
                            Text {
                                width: parent.width
                                color: "#f0d891"; font.pixelSize: 13; font.bold: true
                                text: modelData.name + " (seat " + modelData.seatId + ")"
                                     + (modelData.folded ? " · folded this hand" : "")
                            }
                            Row {
                                width: parent.width
                                spacing: 5
                                TextField {
                                    id: rangeField
                                    width: parent.width - 91
                                    placeholderText: "Any / AA,KK,AKs..."
                                    text: opponentRow.modelData.notation
                                    font.pixelSize: 12
                                    color: "#f6f9ff"
                                    placeholderTextColor: "#b3c6df"
                                    selectionColor: "#4f79a3"
                                    selectedTextColor: "#ffffff"
                                    background: Rectangle {
                                        radius: 6
                                        color: "#293d54"
                                        border.color: rangeField.activeFocus ? "#e2d089" : "#8299b4"
                                        border.width: 1
                                    }
                                    onAccepted: kb.applyRange(opponentRow.modelData.seatId, text)
                                }
                                TrainerButton {
                                    width: 86
                                    text: "Apply"
                                    onClicked: kb.applyRange(opponentRow.modelData.seatId, rangeField.text)
                                }
                            }
                            Text {
                                width: parent.width
                                color: "#b8c9df"; font.pixelSize: 11
                                text: opponentRow.modelData.custom
                                      ? "Selected: " + (100 * opponentRow.modelData.coverage).toFixed(1)
                                        + "% of all 1,326 starting combinations (before blockers)"
                                      : "Selected: any two cards (100%)"
                                wrapMode: Text.WordWrap
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        var combos = Math.round(1326 * opponentRow.modelData.coverage)
                                        kb.rangeFeedback = "Coverage for " + opponentRow.modelData.name
                                             + ": " + combos + " distinct hole-card combinations / "
                                             + "C(52,2) = 1,326 = "
                                             + (100 * opponentRow.modelData.coverage).toFixed(1)
                                             + "%. A pocket pair has 6 combinations, a suited "
                                             + "unpaired hand 4, and an offsuit hand 12. "
                                             + "Actual known cards (blockers) can reduce the usable combinations."
                                    }
                                }
                            }
                            Row {
                                width: parent.width; spacing: 4
                                TrainerButton {
                                    width: (parent.width - 12) / 4
                                    text: "TIGHT"
                                    onClicked: kb.applyRange(opponentRow.modelData.seatId,
                                        "99+,AJs+,AQo+,KQs")
                                }
                                TrainerButton {
                                    width: (parent.width - 12) / 4
                                    text: "MED"
                                    onClicked: kb.applyRange(opponentRow.modelData.seatId,
                                        "66+,ATs+,AJo+,KTs+,KQo,QJs,JTs")
                                }
                                TrainerButton {
                                    width: (parent.width - 12) / 4
                                    text: "WIDE"
                                    onClicked: kb.applyRange(opponentRow.modelData.seatId,
                                        "22+,A2s+,A8o+,K8s+,KTo+,Q9s+,QJo,J9s+,T9s,98s,87s")
                                }
                                TrainerButton {
                                    width: (parent.width - 12) / 4
                                    text: "ANY"
                                    onClicked: kb.applyRange(opponentRow.modelData.seatId, "")
                                }
                            }
                            Rectangle { width: parent.width; height: 1; color: "#566a84" }
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                color: rangeFeedback.indexOf("⚠") === 0 ? "#ffb3b3" : "#c0f5ce"
                font.pixelSize: 11; wrapMode: Text.WordWrap
                text: kb.rangeFeedback
                visible: kb.rangeFeedback !== ""
            }
            Text {
                Layout.fillWidth: true
                color: "#9eb2cf"; font.pixelSize: 11; wrapMode: Text.WordWrap
                text: "Notation: AA = pocket aces; 77+ = pairs 77–AA; "
                      + "AKs = suited ace-king; AJo = offsuit ace-jack; "
                      + "A9s+ = A9s through AKs; 22-55 = pair interval. "
                      + "Combine with commas. Presets are examples, not estimates "
                      + "of what a specific bot actually holds."
            }
        }
    }

    Popup {
        id: explanationPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.max(8, (parent.width - width) / 2)
        y: Math.max(8, (parent.height - height) / 2)
        width: Math.min(570, parent.width - 16)
        height: Math.min(610, parent.height - 16)
        padding: 12
        background: Rectangle { radius: 13; color: "#1c2636"; border.color: "#94a9ca" }
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    color: "#f0d891"
                    font.bold: true
                    font.pixelSize: 16
                    wrapMode: Text.WordWrap
                    text: kb.explanationTitle
                }
                TrainerButton { text: "Close"; onClicked: explanationPopup.close() }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                Text {
                    width: parent.width
                    color: "#e7ecf6"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    text: kb.explanationBody
                }
            }
            TrainerButton {
                Layout.fillWidth: true
                visible: kb.replayEvent !== null
                text: "REPLAY CALCULATION"
                onClicked: kb.doReplay()
            }
            Text {
                Layout.fillWidth: true
                visible: kb.replayResult !== ""
                color: kb.replayResult.indexOf("MISMATCH") >= 0 ? "#ff9c9c" : "#b9f0cb"
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                text: kb.replayResult
            }
        }
    }

    Popup {
        id: progressPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.max(8, (parent.width - width) / 2)
        y: Math.max(8, (parent.height - height) / 2)
        width: Math.min(550, parent.width - 16)
        height: Math.min(710, parent.height - 16)
        padding: 12
        background: Rectangle { radius: 13; color: "#1c2636"; border.color: "#94a9ca" }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: "TRAINING PROGRESS"
                    color: "#f0d891"
                    font.bold: true
                    font.pixelSize: 18
                }
                TrainerButton { text: "Close"; onClicked: progressPopup.close() }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                Column {
                    id: dashboard
                    width: parent.width
                    spacing: 11
                    Text {
                        width: parent.width
                        color: "white"
                        font.pixelSize: 14
                        font.bold: true
                        text: kb.progress.totalHands + " tracked hands · "
                              + kb.progress.sessionHands + " this session ⓘ"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainSummary("Tracked hands and current session",
                                kb.progress.all,
                                "Total tracked hands = hands with recorded decisions. "
                                + "Current session " + kb.progress.sessionId + " has "
                                + kb.progress.sessionHands + " such hands; "
                                + "history is limited to " + kb.progress.historyLimit
                                + " hands, oldest deleted first.")
                        }
                    }
                    Text {
                        width: parent.width
                        color: "#aebfd7"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: "Decision-based training rates use hands with recorded actions. "
                              + "BB/100 uses all settled hands, including blind-only walks. "
                              + "Each rate shows its own denominator."
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text {
                        color: "#f0d891"; font.bold: true
                        text: "Tournament results · local games"
                    }
                    Text {
                        width: parent.width; color: "white"; wrapMode: Text.WordWrap
                        font.pixelSize: 13; font.bold: true
                        text: {
                            var t = kb.progress.tournament || {}
                            return "Wins: " + (t.wins || 0) + "/" + (t.completed || 0)
                                + " completed (" + (t.completed ? kb.fmt(t.winPercent) : "N/A") + ")"
                                + " · avg finish " + (t.completed
                                    ? Number(t.averagePlace).toFixed(2) : "N/A")
                                + " · abandoned " + (t.abandoned || 0) + " ⓘ"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainTournament("Tournament finishes — definitions")
                        }
                    }
                    Text {
                        width: parent.width; color: "#b8efd0"; wrapMode: Text.WordWrap
                        font.pixelSize: 13; font.bold: true
                        text: {
                            var t = kb.progress.tournament || {}
                            return "Realized chip BB/100: "
                                + (t.bbHands ? kb.signedNumber(t.bbPer100,1) : "N/A")
                                + " · " + (t.bbHands || 0) + " settled hands"
                                + " · Σ " + kb.signedNumber(t.bbWon,1) + " BB ⓘ"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainTournament("BB/100 — exact arithmetic and inputs")
                        }
                    }
                    Text {
                        width: parent.width; color: "#b8c9df"; wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: {
                            var t = kb.progress.tournament || {}
                            return "By starting stack depth: >40BB "
                                + (t.deepHands ? kb.signedNumber(t.deepBbPer100,1) : "N/A")
                                + " (" + (t.deepHands || 0) + " hands)"
                                + " · 15–40BB "
                                + (t.mediumHands ? kb.signedNumber(t.mediumBbPer100,1) : "N/A")
                                + " (" + (t.mediumHands || 0) + ")"
                                + " · <15BB "
                                + (t.shortHands ? kb.signedNumber(t.shortBbPer100,1) : "N/A")
                                + " (" + (t.shortHands || 0) + ") ⓘ"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainTournament("Stack-depth normalized BB/100")
                        }
                    }
                    Text {
                        width: parent.width; color: "#aebfd7"; wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: {
                            var t = kb.progress.tournament || {}
                            if (!t.active) return "No local tournament currently in progress."
                            return "Current tournament: " + t.currentEntrants
                                + " entrants · " + t.currentHands + " settled hands"
                                + " · stack " + t.currentStack
                                + " · " + (t.currentHands
                                    ? kb.signedNumber(t.currentBbPer100,1) : "N/A")
                                + " BB/100 so far."
                        }
                    }
                    Text {
                        color: "#f0d891"; font.bold: true
                        text: "Recent tournament finishes"
                    }
                    Repeater {
                        model: (kb.progress.tournament || {}).recentFinishes || []
                        delegate: Text {
                            required property var modelData
                            width: dashboard.width
                            color: "#d9e5f6"; wrapMode: Text.WordWrap; font.pixelSize: 11
                            text: modelData.completed
                                ? "#" + modelData.place + "/" + modelData.entrants
                                    + (modelData.tiedPlace ? " (tied)" : "")
                                    + " · " + modelData.hands + " hands"
                                    + " · " + kb.signedNumber(modelData.bbWon,1) + " BB"
                                    + " · " + modelData.finishedAt + " ⓘ"
                                : "Abandoned · " + modelData.hands + " hands"
                                    + " · " + modelData.finishedAt + " ⓘ"
                            MouseArea {
                                anchors.fill: parent
                                onClicked: kb.explainTournamentRecord(modelData)
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        color: "#9fb0ca"; wrapMode: Text.WordWrap; font.pixelSize: 11
                        text: "BB/100 measures chip accumulation, not tournament prize value. "
                            + "Old hands without an opening/closing stack cannot be reconstructed."
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text {
                        width: parent.width; color: "#cee7c5"; wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        text: (kb.progress.trainingFocus || "") + " ⓘ"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainSummary("Training focus — underlying evidence",
                                kb.progress.all, kb.progress.trainingFocus)
                        }
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text { color: "#f0d891"; font.bold: true; text: "Last 20 hands vs preceding 20" }
                    Text {
                        width: parent.width; color: "white"; wrapMode: Text.WordWrap
                        text: "Free-check folds: "
                              + kb.progress.last20.clearMistakes + "/"
                              + kb.progress.last20.freeOpportunities + " ("
                              + kb.fmt(kb.progress.last20.freeFoldPercent) + ")"
                              + (kb.progress.canCompare20 ? "  · previously "
                                 + kb.progress.previous20.clearMistakes + "/"
                                 + kb.progress.previous20.freeOpportunities + " ("
                                 + kb.fmt(kb.progress.previous20.freeFoldPercent) + ")"
                                 : "  · need 40 tracked hands for comparison") + " ⓘ"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainSummary("Free-check folds — last 20 hands",
                                kb.progress.last20,
                                "Last 20 tracked hands compared with the preceding 20. "
                                + "Free-check-fold rate = clear mistakes / opportunities "
                                + "to take a free check. The preceding window has "
                                + kb.progress.previous20.clearMistakes + " such mistakes / "
                                + kb.progress.previous20.freeOpportunities + " opportunities. "
                                + "Comparison requires 40 tracked hands.")
                        }
                    }
                    Text {
                        width: parent.width; color: "#f7ddb0"; wrapMode: Text.WordWrap
                        text: "Model review warnings: "
                              + kb.progress.last20.reviewWarnings + "/"
                              + kb.progress.last20.pricedReviewed + " ("
                              + kb.fmt(kb.progress.last20.reviewPercent) + ")"
                              + (kb.progress.canCompare20 ? "  · previously "
                                 + kb.progress.previous20.reviewWarnings + "/"
                                 + kb.progress.previous20.pricedReviewed + " ("
                                 + kb.fmt(kb.progress.previous20.reviewPercent) + ")" : "") + " ⓘ"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainSummary("Model warnings — last 20 hands",
                                kb.progress.last20,
                                "Warnings / graded paid calls or folds. These are NOT proven mistakes. "
                                + "Previous window: " + kb.progress.previous20.reviewWarnings
                                + "/" + kb.progress.previous20.pricedReviewed
                                + ". Both windows contain at most 20 tracked hands.")
                        }
                    }
                    Text {
                        width: parent.width; color: "#aebfd7"; wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: "Review flags use random or user-assigned ranges, not proof of bad play. "
                              + "Small sample differences can be ordinary noise."
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text { color: "#f0d891"; font.bold: true; text: "Preflop tendencies — tracked hands" }
                    Text {
                        width: parent.width; color: "white"; wrapMode: Text.WordWrap
                        text: "VPIP " + kb.fmt(kb.progress.all.vpipPercent)
                              + " (" + kb.progress.all.vpipHands + "/"
                              + kb.progress.all.preflopObservedHands + ")"
                              + " · PFR " + kb.fmt(kb.progress.all.pfrPercent)
                              + " (" + kb.progress.all.pfrHands + "/"
                              + kb.progress.all.preflopObservedHands + ") ⓘ"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: kb.explainSummary("VPIP and PFR — full history",
                                kb.progress.all,
                                "Both are proportions of tracked hands with a preflop action, "
                                + "not all hands dealt or all opportunities.")
                        }
                    }
                    Text {
                        width: parent.width; color: "#aebfd7"; wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: "VPIP = voluntarily played a preflop hand; PFR = preflop raise. "
                              + "Blinds excluded; hands with no preflop action were not observed. "
                              + "No universal ideal percentage without table context."
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text { color: "#f0d891"; font.bold: true; text: "Preflop by position" }
                    Repeater {
                        model: kb.progress.positions || []
                        delegate: Text {
                            required property var modelData
                            width: dashboard.width; color: "#d7e3f6"; wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            text: modelData.position + ": VPIP " + kb.fmt(modelData.vpipPercent)
                                  + " · PFR " + kb.fmt(modelData.pfrPercent)
                                  + " · " + modelData.preflopObservedHands + " observed hands ⓘ"
                            MouseArea {
                                anchors.fill: parent
                                onClicked: kb.explainSummary("Preflop position: " + modelData.position,
                                    modelData, "BTN = dealer, SB/BB = blinds, Other = remaining seats.")
                            }
                        }
                    }
                    Text { width: parent.width; color: "#aebfd7"; wrapMode: Text.WordWrap
                           font.pixelSize: 10
                           text: "BTN = dealer, SB/BB = blinds; Other groups all remaining seats." }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text { color: "#f0d891"; font.bold: true; text: "Where you may be improving — by street" }
                    Repeater {
                        model: kb.progress.streets || []
                        delegate: Column {
                            required property var modelData
                            width: dashboard.width
                            spacing: 2
                            Text {
                                color: "white"; font.bold: true
                                text: modelData.street + " · " + modelData.last50.decisions
                                      + " decisions in last 50 hands ⓘ"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: kb.explainSummary(modelData.street + " — last 50 hands",
                                        modelData.last50,
                                        "Only decisions on this street are counted. "
                                        + "Previous 50 decisions: " + modelData.previous50.decisions)
                                }
                            }
                            Text {
                                width: parent.width; color: "#d7e3f6"
                                font.pixelSize: 11; wrapMode: Text.WordWrap
                                text: "Free-fold errors " + modelData.last50.clearMistakes
                                      + "/" + modelData.last50.freeOpportunities
                                      + " · model warnings " + modelData.last50.reviewWarnings
                                      + "/" + modelData.last50.pricedReviewed + " ⓘ"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: kb.explainSummary(modelData.street + " — last 50 hands",
                                        modelData.last50,
                                        "This row contains counts, not percentages; divide each "
                                        + "numerator by its adjacent denominator.")
                                }
                            }
                            Text {
                                visible: kb.progress.canCompare50
                                width: parent.width; color: "#9fb0ca"
                                font.pixelSize: 11; wrapMode: Text.WordWrap
                                text: "Prior 50: errors " + modelData.previous50.clearMistakes
                                      + "/" + modelData.previous50.freeOpportunities
                                      + " · warnings " + modelData.previous50.reviewWarnings
                                      + "/" + modelData.previous50.pricedReviewed + " ⓘ"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: kb.explainSummary(modelData.street + " — preceding 50 hands",
                                        modelData.previous50,
                                        "Previous 50 tracked hands, grouped by street.")
                                }
                            }
                        }
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text { color: "#f0d891"; font.bold: true; text: "History · last 120 hands, groups of 10" }
                    Text { width: parent.width; color: "#aebfd7"; font.pixelSize: 11
                           text: "Gold: model warnings / scored calls or folds · Red: free-check errors / free options" }
                    Flickable {
                        width: parent.width
                        height: 92
                        contentWidth: bucketRow.width
                        clip: true
                        Row {
                            id: bucketRow
                            spacing: 10
                            Repeater {
                                model: kb.progress.buckets || []
                                delegate: Column {
                                    required property var modelData
                                    width: 28; spacing: 3
                                    Item {
                                        width: 28; height: 58
                                        Rectangle {
                                            anchors.bottom: parent.bottom
                                            x: 2; width: 10
                                            color: "#e6bf69"
                                            height: modelData.pricedReviewed > 0
                                                    ? Math.max(3, 55 * modelData.reviewPercent / 100) : 2
                                        }
                                        Rectangle {
                                            anchors.bottom: parent.bottom
                                            x: 16; width: 10
                                            color: "#e47777"
                                            height: modelData.freeOpportunities > 0
                                                    ? Math.max(3, 55 * modelData.freeFoldPercent / 100) : 2
                                        }
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        color: "#c7d5eb"; font.pixelSize: 10
                                        text: String(modelData.to)
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: kb.explainSummary(
                                            "History bucket " + modelData.from + "–" + modelData.to,
                                            modelData, "Each bar represents up to ten tracked hands. "
                                            + "Gold = model warnings / graded paid decisions. "
                                            + "Red = free-check folds / free-check opportunities. "
                                            + "Height = rate as a percentage of 55 display pixels.")
                                    }
                                }
                            }
                        }
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text { color: "#f0d891"; font.bold: true; text: "Practice sessions" }
                    Repeater {
                        model: kb.progress.sessions || []
                        delegate: Text {
                            required property var modelData
                            width: dashboard.width; color: "white"; wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            text: "Session " + modelData.id + ": " + modelData.hands
                                  + " hands · " + modelData.clearMistakes + " free-fold mistakes"
                                  + " · " + modelData.reviewWarnings + "/"
                                  + modelData.pricedReviewed + " model warnings ⓘ"
                            MouseArea {
                                anchors.fill: parent
                                onClicked: kb.explainSummary("Practice session " + modelData.id,
                                    modelData,
                                    "From " + modelData.fromDate + " to " + modelData.toDate)
                            }
                        }
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Row {
                        spacing: 8
                        Text {
                            text: "Recorded decisions — tap to inspect/replay"
                            color: "#f0d891"; font.bold: true; font.pixelSize: 12
                        }
                        TrainerButton { text: "Refresh"; onClicked: kb.refreshHistory() }
                    }
                    Repeater {
                        model: kb.recentDecisions || []
                        delegate: Text {
                            required property var modelData
                            width: dashboard.width; color: "#d7e3f6"; wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            text: "Hand " + modelData.hand + " · " + modelData.phase
                                  + " · " + modelData.action
                                  + " · " + modelData.rating + " ⓘ"
                            MouseArea {
                                anchors.fill: parent
                                onClicked: kb.explainDecision(modelData)
                            }
                        }
                    }
                    Rectangle { width: parent.width; height: 1; color: "#556781" }
                    Text { color: "#f0d891"; font.bold: true; text: "Local history backup" }
                    Text {
                        width: parent.width; color: "#c2d2e9"; wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: "History lives in private app storage. Uninstalling the Kibitzer app "
                              + "erases it. Use the same signing key for updates, or copy "
                              + "your JSON backup before uninstalling. Restore replaces current history."
                    }
                    TrainerButton {
                        text: "Copy history JSON"
                        onClicked: kb.backupMessage = GameTable.kibitzerCopyBackup()
                                   ? "History copied to clipboard" : "Backup unavailable"
                    }
                    TextArea {
                        id: restoreArea
                        width: parent.width
                        height: 74
                        placeholderText: "Paste a Kibitzer JSON backup to restore it"
                        color: "white"
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                        background: Rectangle { color: "#33435a"; radius: 6 }
                        onTextChanged: kb.confirmRestore = false
                    }
                    TrainerButton {
                        text: kb.confirmRestore ? "CONFIRM: replace saved history" : "Restore pasted history (replace)"
                        enabled: restoreArea.text.length > 0
                        onClicked: {
                            if (!kb.confirmRestore) { kb.confirmRestore = true; return }
                            kb.backupMessage = GameTable.kibitzerImportBackup(restoreArea.text)
                                             ? "Backup restored" : "Invalid backup or storage error"
                            kb.confirmRestore = false
                        }
                    }
                    Text {
                        width: parent.width; color: "#f8c6a3"; wrapMode: Text.WordWrap
                        font.pixelSize: 11
                        text: kb.backupMessage || (kb.progress.storageError || "")
                    }
                    Text {
                        width: parent.width; color: "#9fb0ca"; wrapMode: Text.WordWrap
                        font.pixelSize: 10
                        text: kb.progress.disclaimer || ""
                    }
                }
            }
        }
    }
}
