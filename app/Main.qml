import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: root
    width: 640
    height: 760
    visible: true
    color: "#000000"
    title: "pacman-qt"

    property int cols: Math.max(game.mazeCols, 1)
    property int rows: Math.max(game.mazeRows, 1)
    property real cell: Math.max(Math.min(boardArea.width / cols, boardArea.height / rows), 4)

    // Frame-gap instrumentation (stalls are logged while the game runs) —
    // kept from the on-device audio investigation, see SfxPlayer.
    property real lastFrameAt: 0

    // The pause menu (the "back" menu on a phone). It pauses the game while
    // it is open and restores the previous state when it closes.
    property bool menuOpen: false
    property bool menuPausedTheGame: false

    function openMenu() {
        if (menuOpen) {
            return
        }
        menuOpen = true
        if (game.stateName === "running" && !game.paused) {
            game.togglePause()
            menuPausedTheGame = true
        }
    }

    function closeMenu() {
        if (!menuOpen) {
            return
        }
        menuOpen = false
        if (menuPausedTheGame) {
            game.togglePause()
            menuPausedTheGame = false
        }
    }

    Column {
        id: rootColumn
        anchors.fill: parent
        // Keep the HUD and the board clear of notches, rounded corners and
        // system bars (Android reports the safe area; elsewhere it is empty).
        anchors.leftMargin: insets.left
        anchors.rightMargin: insets.right
        anchors.topMargin: insets.top
        anchors.bottomMargin: insets.bottom
        spacing: 0
        focus: true

        Item {
            id: hud
            width: parent.width
            height: 60

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 28

                Column {
                    spacing: 2
                    Text {
                        text: "SCORE"
                        color: "#888888"
                        font.pixelSize: 11
                    }
                    Text {
                        text: game.score
                        color: "#ffffff"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
                Column {
                    spacing: 2
                    Text {
                        text: "HIGH SCORE"
                        color: "#888888"
                        font.pixelSize: 11
                    }
                    Text {
                        text: game.highScore
                        color: "#ffffff"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
                Column {
                    spacing: 2
                    Text {
                        text: "LEVEL"
                        color: "#888888"
                        font.pixelSize: 11
                    }
                    Text {
                        text: game.levelName.length > 0 ? game.levelName : "-"
                        color: "#ffffff"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Repeater {
                    model: Math.max(game.lives, 0)
                    Image {
                        source: "qrc:/assets/sprites/life.svg"
                        width: 20
                        height: 20
                        fillMode: Image.PreserveAspectFit
                    }
                }

                // The menu button: on a phone there is no keyboard, so the
                // pause menu has to be reachable by touch.
                Rectangle {
                    id: menuButton
                    width: 34
                    height: 28
                    radius: 4
                    color: "#101010"
                    border.color: "#ffff00"
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "≡"
                        color: "#ffff00"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.menuOpen ? root.closeMenu() : root.openMenu()
                    }
                }
            }
        }

        Item {
            id: boardArea
            width: parent.width
            height: parent.height - hud.height

            Canvas {
                id: board
                width: root.cell * root.cols
                height: root.cell * root.rows
                anchors.centerIn: parent

                function circle(ctx, x, y, r, color) {
                    ctx.fillStyle = color
                    ctx.beginPath()
                    ctx.arc(x, y, r, 0, Math.PI * 2)
                    ctx.closePath()
                    ctx.fill()
                }

                function drawEyes(ctx, px, py, c) {
                    const dx = c / 6
                    const dy = -c / 10
                    circle(ctx, px - dx, py + dy, c * 2 / 15, "#ffffff")
                    circle(ctx, px + dx, py + dy, c * 2 / 15, "#ffffff")
                    circle(ctx, px - dx, py + dy, c / 15, "#000000")
                    circle(ctx, px + dx, py + dy, c / 15, "#000000")
                }

                function drawPlayer(ctx, c) {
                    const px = game.playerX * c
                    const py = game.playerY * c
                    const r = Math.max(c / 2 - 2, 1)
                    let base = 0
                    switch (game.playerDirection) {
                    case 1: base = 1.5 * Math.PI; break
                    case 2: base = 0; break
                    case 3: base = 0.5 * Math.PI; break
                    case 4: base = Math.PI; break;
                    }
                    const mouth = Math.abs(Math.sin(game.playerMouth * Math.PI * 2)) * 30 * Math.PI / 180
                    ctx.fillStyle = game.stateName === "died" ? "#ff0000" : "#ffff00"
                    ctx.beginPath()
                    ctx.moveTo(px, py)
                    ctx.arc(px, py, r, base + mouth, base + 2 * Math.PI - mouth)
                    ctx.closePath()
                    ctx.fill()
                }

                function drawGhost(ctx, c, g) {
                    const px = g.x * c
                    const py = g.y * c
                    const r = Math.max(c / 2 - 2, 1)
                    const colors = ["#ff0000", "#ffb3b3", "#00ffff", "#ff9900"]
                    if (!g.eyes) {
                        ctx.fillStyle = g.edible ? (g.flashing ? "#ffffff" : "#0000ff") : colors[g.id]
                        ctx.beginPath()
                        ctx.arc(px, py, r, Math.PI, 2 * Math.PI)
                        ctx.rect(px - r, py, 2 * r, r)
                        ctx.closePath()
                        ctx.fill()
                    }
                    drawEyes(ctx, px, py, c)
                }

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.fillStyle = "#000000"
                    ctx.fillRect(0, 0, width, height)
                    const c = width / root.cols
                    const walls = game.mazeData
                    const items = game.itemData
                    const total = game.mazeCols * game.mazeRows
                    ctx.lineWidth = Math.max(c * 2 / 30, 1)
                    ctx.strokeStyle = "#0000ff"
                    for (let i = 0; i < total; ++i) {
                        const x = i % game.mazeCols
                        const y = (i - x) / game.mazeCols
                        const wall = walls.charAt(i)
                        if (wall === "#") {
                            const inset = c / 15
                            ctx.strokeRect(x * c + inset, y * c + inset, c - 2 * inset, c - 2 * inset)
                        } else if (wall === "-") {
                            ctx.fillStyle = "#ffb8de"
                            ctx.fillRect(x * c + c * 0.1, y * c + c * 0.35, c * 0.8, c * 0.25)
                        }
                        const item = items.charAt(i)
                        if (item === ".") {
                            circle(ctx, (x + 0.5) * c, (y + 0.5) * c, Math.max(c / 15, 1), "#ffffff")
                        } else if (item === "o") {
                            circle(ctx, (x + 0.5) * c, (y + 0.5) * c, Math.max(c / 5, 2), "#ffffff")
                        } else if (item === "F") {
                            circle(ctx, (x + 0.5) * c, (y + 0.5) * c, Math.max(c * 0.3, 3), "#ff3131")
                        }
                    }
                    drawPlayer(ctx, c)
                    const list = game.ghosts
                    for (let i = 0; i < list.length; ++i) {
                        drawGhost(ctx, c, list[i])
                    }
                }

                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Component.onCompleted: requestPaint()

                Connections {
                    target: game
                    function onActorsChanged() {
                        board.requestPaint()
                    }
                    function onMazeChanged() {
                        board.requestPaint()
                    }
                    function onItemsChanged() {
                        board.requestPaint()
                    }
                    function onStateChanged() {
                        board.requestPaint()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: game.mazeCols === 0
                text: "NO LEVELS FOUND"
                color: "#ff0000"
                font.pixelSize: 22
                font.bold: true
            }

            Text {
                anchors.centerIn: board
                visible: game.stateName === "ready"
                text: "READY!"
                color: "#ffff00"
                font.pixelSize: Math.max(root.cell, 14)
                font.bold: true
            }

            Text {
                anchors.centerIn: board
                anchors.verticalCenterOffset: Math.max(root.cell * 1.9, 34)
                visible: game.stateName === "ready" || game.stateName === "gameOver"
                text: game.stateName === "ready" ? "TAP OR SPACE TO START — SWIPE OR ARROWS TO TURN"
                                                 : "TAP OR SPACE TO PLAY AGAIN"
                color: "#888888"
                font.pixelSize: Math.max(root.cell * 0.5, 12)
            }

            Text {
                anchors.centerIn: board
                visible: game.stateName === "levelComplete"
                text: "LEVEL CLEAR"
                color: "#00ffff"
                font.pixelSize: Math.max(root.cell, 14)
                font.bold: true
            }

            Text {
                anchors.centerIn: board
                visible: game.stateName === "gameOver"
                text: "GAME OVER"
                color: "#ff0000"
                font.pixelSize: Math.max(root.cell, 14)
                font.bold: true
            }

            Text {
                anchors.centerIn: board
                visible: game.paused && !root.menuOpen
                text: "PAUSED"
                color: "#ffffff"
                font.pixelSize: Math.max(root.cell, 14)
                font.bold: true
            }

            Text {
                id: popup
                property real tileX: 0
                property real tileY: 0
                x: (tileX + 0.5) * root.cell - width / 2
                y: tileY * root.cell - root.cell * 0.4
                visible: opacity > 0.01
                opacity: 0
                color: "#ffffff"
                font.pixelSize: Math.max(root.cell * 0.6, 11)
                font.bold: true

                function show(label, tx, ty) {
                    popup.text = label
                    popup.tileX = tx
                    popup.tileY = ty
                    popupFade.restart()
                }

                NumberAnimation {
                    id: popupFade
                    target: popup
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: 900
                }

                Connections {
                    target: game
                    function onGhostEaten(points, x, y) {
                        popup.show("+" + points, x, y)
                    }
                    function onFruitEaten(points, x, y) {
                        popup.show("+" + points, x, y)
                    }
                }
            }

            // Touch controls (phones/tablets): swipe or drag to turn, tap to
            // start or restart — the touch equivalents of the arrow keys and
            // Space. The threshold is the dead zone that keeps a tap (or a
            // shaky finger) from spinning Pac-Man; once crossed the origin
            // re-arms, so a continuous drag keeps steering.
            MouseArea {
                id: touchControl
                anchors.fill: parent
                property real pressX: 0
                property real pressY: 0
                property bool swiped: false
                readonly property real threshold: Math.max(30, Math.min(boardArea.width, boardArea.height) * 0.08)

                function sendDirection(dx, dy) {
                    if (Math.abs(dx) > Math.abs(dy)) {
                        game.requestDirection(dx > 0 ? 2 : 4)   // right : left
                    } else {
                        game.requestDirection(dy > 0 ? 3 : 1)   // down  : up
                    }
                }

                onPressed: (mouse) => {
                    pressX = mouse.x
                    pressY = mouse.y
                    swiped = false
                }
                onPositionChanged: (mouse) => {
                    if (!pressed) {
                        return
                    }
                    const dx = mouse.x - pressX
                    const dy = mouse.y - pressY
                    if (Math.max(Math.abs(dx), Math.abs(dy)) >= threshold) {
                        swiped = true
                        sendDirection(dx, dy)
                        pressX = mouse.x
                        pressY = mouse.y
                    }
                }
                onReleased: (mouse) => {
                    if (!swiped) {
                        game.start()      // tap == Space: start / restart
                        return
                    }
                    const dx = mouse.x - pressX
                    const dy = mouse.y - pressY
                    if (Math.max(Math.abs(dx), Math.abs(dy)) >= threshold) {
                        sendDirection(dx, dy)
                    }
                }
            }
        }

        Connections {
            target: game
            function onPelletEaten(x, y) {
                sfx.playMunch()      // cheap: playback happens in SfxPlayer's thread
            }
            function onPowerPelletEaten(x, y) {
                sfx.playPower()
            }
            function onGhostEaten(points, x, y) {
                sfx.playGhostEaten()
            }
            function onPlayerDied() {
                sfx.playDeath()
            }
        }

        // Frame pacing: log any stall longer than 45 ms while the game is
        // running, so audio-related hitches can be correlated with the
        // play() timings below in logcat.
        Connections {
            target: game
            function onActorsChanged() {
                const now = Date.now()
                if (lastFrameAt > 0 && game.stateName === "running"
                        && now - lastFrameAt > 45) {
                    console.log("frame gap " + (now - lastFrameAt) + "ms")
                }
                lastFrameAt = now
            }
        }

        Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up:
        case Qt.Key_W:
            game.requestDirection(1)
            break
        case Qt.Key_Right:
        case Qt.Key_D:
            game.requestDirection(2)
            break
        case Qt.Key_Down:
        case Qt.Key_S:
            game.requestDirection(3)
            break
        case Qt.Key_Left:
        case Qt.Key_A:
            game.requestDirection(4)
            break
        case Qt.Key_Space:
        case Qt.Key_Return:
            game.start()
            break
        case Qt.Key_P:
            game.togglePause()
            break
        case Qt.Key_M:
            game.settings.toggleSound()
            break
        case Qt.Key_R:
            game.reset()
            break
        case Qt.Key_Escape:
        // The Android back button arrives as Qt.Key_Back; both open the
        // pause menu instead of leaving the game.
        case Qt.Key_Back:
            root.menuOpen ? root.closeMenu() : root.openMenu()
            break
        default:
            return
        }
        event.accepted = true
        }
    }

    // The Android activity forwards the back button here (Android swallows
    // it before it reaches the Qt window), so the phone gets the same pause
    // menu as the Escape key.
    Connections {
        target: android
        function onBackPressed() {
            root.menuOpen ? root.closeMenu() : root.openMenu()
        }
    }

    // The pause menu. It lives outside the column so that it covers the
    // whole window, and it swallows touches so that a tap on the menu does
    // not steer Pac-Man.
    Rectangle {
        id: menu
        anchors.fill: parent
        color: "#cc000000"
        visible: root.menuOpen

        MouseArea {
            anchors.fill: parent
            onClicked: {}      // swallow: no steering while the menu is open
        }

        Column {
            anchors.centerIn: parent
            spacing: 14

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "PAUSED"
                color: "#ffff00"
                font.pixelSize: 28
                font.bold: true
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: game.settings.soundEnabled ? "SOUND: ON" : "SOUND: OFF"
                color: "#ffffff"
                font.pixelSize: 18

                MouseArea {
                    anchors.fill: parent
                    onClicked: game.settings.toggleSound()
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: game.settings.slowMode ? "SPEED: SLOW" : "SPEED: NORMAL"
                color: "#ffffff"
                font.pixelSize: 18

                MouseArea {
                    anchors.fill: parent
                    onClicked: game.settings.toggleSlowMode()
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "RESUME"
                color: "#00ffff"
                font.pixelSize: 18

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.closeMenu()
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "RESTART"
                color: "#ff8080"
                font.pixelSize: 18

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.closeMenu()
                        game.reset()
                    }
                }
            }
        }
    }
}
