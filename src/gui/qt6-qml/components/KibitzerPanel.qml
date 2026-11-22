import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Local-only training adviser and review. Does not send poker actions.
Rectangle {
    id: kb
    color: "#eb19212d"
    radius: 10
    border.width: 1
    border.color: "#a2b3d0"
    property var advice: ({"status": "unavailable"})
    property bool expanded: false
    property bool openRequested: false
    readonly property bool editingRestore: restoreArea.activeFocus
    readonly property bool popupOpen: explanationPopup.visible || progressPopup.visible || rangePopup.visible
    property var rangePlayers: []
    property string rangeFeedback: ""
    readonly property bool customRangeActive: advice.rangeStatus === "ready"
    function refreshOpponentRanges() {
        rangePlayers = GameTable ? GameTable.kibitzerOpponentRanges() : []
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
    function describeRangeScenario() {
        var a = advice
        if (a.status !== "ready") {
            explain("Range sensitivity", "No live equity calculation is available.")
            return
        }
        var body = "Baseline against uniformly random hands: "
            + fmt(100*a.equity) + ".\\n"
        if (a.rangeStatus === "baseline") {
            body += "No active opponent has a manual range. Use RANGES to "
                + "set a hypothesis for each seat; leave blank for random.\\n"
        } else if (a.rangeStatus === "ready") {
            var b = a.rangeScenario
            body += "Conditioned equity under your assumptions: "
                 + fmt(100*a.rangeEquity) + "; change from random: "
                 + (a.rangeDelta >= 0 ? "+" : "") + (100*a.rangeDelta).toFixed(1)
                 + " percentage points.\\n"
            body += "Monte Carlo samples: " + b.samples + "/" + b.samplesRequested
                 + "; seed: " + b.seed
                 + "; sampling standard error: "
                 + (100*a.rangeStdError).toFixed(2) + " percentage points.\\n"
        } else {
            body += "Range scenario unavailable: " + (a.rangeMessage || "insufficient compatible deals") + "\\n"
        }
        var rows = a.activeRanges || []
        body += "\\nActive opponents and assumptions:\\n"
        for (var i = 0; i < rows.length; ++i)
            body += rows[i].name + " (seat " + rows[i].seatId + "): "
                  + (rows[i].notation || "ANY / random") + "\\n"
        body += "\\nRanges cover starting-hand TYPES, not fixed cards. "
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
                    : (expanded ? 142 : (myTurnNow && advice.rangeStatus !== "baseline" ? 100 : 76))
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
            body += "\\nA separate estimate uses your manual opponent ranges. Tap its "
                 + "value to inspect each range and the sampler. "
                 + "The baseline remains available for comparison.\\n"
        body += "Showdown equity is not full expected value. Future betting, fold equity, "
             + "side pots and tournament ICM are excluded. Recommendations use a "
             + "heuristic 12-percentage-point cushion, not a confidence test."
        kb.explain("Equity — method and uncertainty", body)
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
            ? "Recomputed equity: " + kb.fmt(100 * r.equity)
              + "; original: " + kb.fmt(100 * r.originalEquity)
              + "; samples " + r.samples
              + "; deterministic replay: " + (r.matched ? "MATCH" : "MISMATCH")
            : "Cannot replay: " + (r.reason || "unknown reason")
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
                width: parent.width - 145
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
                width: 96
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
                text: "Equity: " + kb.fmt(100*kb.advice.equity) + " ⓘ"
                MouseArea { anchors.fill: parent; onClicked: kb.explainEquity() }
            }
            Text {
                width: (parent.width - 8) / 2
                color: "#d9e5f6"; font.pixelSize: 11; elide: Text.ElideRight
                text: "Call threshold: " + kb.fmt(100*kb.advice.required) + " ⓘ"
                MouseArea { anchors.fill: parent; onClicked: kb.explainThreshold() }
            }
        }
        Text {
            visible: kb.expanded && !kb.reviewMode && kb.advice.status === "ready"
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#d9e5f6"
            font.pixelSize: 11
            text: kb.advice.explanation + " (No fold equity or tournament ICM.)"
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
                Button { text: "Close"; onClicked: explanationPopup.close() }
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
            Button {
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
                Button { text: "Close"; onClicked: progressPopup.close() }
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
                        text: "Only hands with your recorded decisions count. "
                              + "All rates show their denominators; no artificial 'skill score'."
                    }
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
                        text: "Review flags are based on random opponent ranges, not proof of bad play. "
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
                        Button { text: "Refresh"; onClicked: kb.refreshHistory() }
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
                    Button {
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
                    Button {
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
