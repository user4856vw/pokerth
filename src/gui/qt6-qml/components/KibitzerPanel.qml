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
    property bool forceReview: false
    property string backupMessage: ""
    property bool confirmRestore: false
    readonly property bool myTurnNow: GameTable && (GameTable.myTurn || GameTable.awaitingMyAction)
    readonly property var report: GameTable ? GameTable.kibitzerReview : ({"entries": []})
    readonly property var progress: GameTable ? GameTable.kibitzerProgress : ({"totalHands": 0})
    readonly property bool hasReview: report && report.entries && report.entries.length > 0
    readonly property bool reviewMode: forceReview || !myTurnNow
    implicitHeight: (expanded && reviewMode) ? Math.min(288, 95 + reviewLines.implicitHeight)
                    : (expanded ? 118 : 76)
    visible: GameTable && !GameTable.spectating && GameTable.isLocalGameRunning()
             && (myTurnNow || hasReview || (progress && progress.totalHands > 0))
    function fmt(n) { return Number(n || 0).toFixed(1) + "%" }

    function refreshAdvice() {
        if (visible && myTurnNow && GameTable)
            advice = GameTable.kibitzerAdvice()
    }
    Timer { id: debounce; interval: 180; onTriggered: kb.refreshAdvice() }
    onMyTurnNowChanged: {
        forceReview = false
        if (myTurnNow) debounce.restart()
    }
    onVisibleChanged: if (visible && myTurnNow) debounce.restart()
    Connections {
        target: GameTable
        function onPlayersChanged() { if (kb.myTurnNow) debounce.restart() }
        function onBoardCardsChanged() { if (kb.myTurnNow) debounce.restart() }
        function onCallAmountChanged() { if (kb.myTurnNow) debounce.restart() }
        function onTotalPotChanged() { if (kb.myTurnNow) debounce.restart() }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4
        Row {
            width: parent.width
            spacing: 7
            Text {
                width: parent.width - 103
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
                MouseArea { anchors.fill: parent; onClicked: progressPopup.open() }
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
        Text {
            visible: !kb.reviewMode
            width: parent.width
            color: "#d9e5f6"
            font.pixelSize: 11
            elide: Text.ElideRight
            text: kb.advice.status === "ready"
                  ? "Equity vs random: " + kb.fmt(100*kb.advice.equity)
                    + " · Call threshold: " + kb.fmt(100*kb.advice.required)
                  : ""
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
                              + modelData.action + ": " + modelData.explanation
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
                              + kb.progress.sessionHands + " this session"
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
                        text: kb.progress.trainingFocus || ""
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
                                 : "  · need 40 tracked hands for comparison")
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
                                 + kb.fmt(kb.progress.previous20.reviewPercent) + ")" : "")
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
                              + kb.progress.all.preflopObservedHands + ")"
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
                                  + " · " + modelData.preflopObservedHands + " observed hands"
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
                                      + " decisions in last 50 hands"
                            }
                            Text {
                                width: parent.width; color: "#d7e3f6"
                                font.pixelSize: 11; wrapMode: Text.WordWrap
                                text: "Free-fold errors " + modelData.last50.clearMistakes
                                      + "/" + modelData.last50.freeOpportunities
                                      + " · model warnings " + modelData.last50.reviewWarnings
                                      + "/" + modelData.last50.pricedReviewed
                            }
                            Text {
                                visible: kb.progress.canCompare50
                                width: parent.width; color: "#9fb0ca"
                                font.pixelSize: 11; wrapMode: Text.WordWrap
                                text: "Prior 50: errors " + modelData.previous50.clearMistakes
                                      + "/" + modelData.previous50.freeOpportunities
                                      + " · warnings " + modelData.previous50.reviewWarnings
                                      + "/" + modelData.previous50.pricedReviewed
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
                                  + modelData.pricedReviewed + " model warnings"
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
