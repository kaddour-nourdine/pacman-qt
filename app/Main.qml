import QtQuick
import QtQuick.Window

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

    Column {
        id: rootColumn
        anchors.fill: parent
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
                spacing: 6

                Repeater {
                    model: Math.max(game.lives, 0)
                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        color: "#ffff00"
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

                function drawEyes(ctx, gx, gy, r) {
                    circle(ctx, gx - r * 0.35, gy - r * 0.2, r * 0.3, "#ffffff")
                    circle(ctx, gx + r * 0.35, gy - r * 0.2, r * 0.3, "#ffffff")
                    circle(ctx, gx - r * 0.28, gy - r * 0.15, r * 0.15, "#2121de")
                    circle(ctx, gx + r * 0.42, gy - r * 0.15, r * 0.15, "#2121de")
                }

                function drawPlayer(ctx, c) {
                    const px = game.playerX * c
                    const py = game.playerY * c
                    const radius = c * 0.42
                    const openness = 0.06 + 0.22 * Math.abs(Math.sin(game.playerMouth * Math.PI * 2))
                    let angle = 0
                    switch (game.playerDirection) {
                    case 1:
                        angle = -Math.PI / 2
                        break
                    case 2:
                        angle = 0
                        break
                    case 3:
                        angle = Math.PI / 2
                        break
                    case 4:
                        angle = Math.PI
                        break
                    }
                    ctx.fillStyle = "#ffff00"
                    ctx.beginPath()
                    ctx.moveTo(px, py)
                    ctx.arc(px, py, radius, angle + openness, angle + Math.PI * 2 - openness)
                    ctx.closePath()
                    ctx.fill()
                }

                function drawGhosts(ctx, c) {
                    const colors = ["#ff0000", "#ffb8ff", "#00ffff", "#ffb852"]
                    const list = game.ghosts
                    for (let i = 0; i < list.length; ++i) {
                        const g = list[i]
                        const gx = g.x * c
                        const gy = g.y * c
                        const r = c * 0.42
                        if (g.eyes) {
                            drawEyes(ctx, gx, gy, r)
                            continue
                        }
                        let color = colors[g.id]
                        if (g.edible) {
                            color = g.flashing ? "#ffffff" : "#2121de"
                        }
                        ctx.fillStyle = color
                        ctx.beginPath()
                        ctx.arc(gx, gy - r * 0.1, r, Math.PI, 0)
                        ctx.lineTo(gx + r, gy + r * 0.85)
                        ctx.lineTo(gx - r, gy + r * 0.85)
                        ctx.closePath()
                        ctx.fill()
                        if (g.edible) {
                            circle(ctx, gx - r * 0.3, gy - r * 0.25, r * 0.14, "#ffffff")
                            circle(ctx, gx + r * 0.3, gy - r * 0.25, r * 0.14, "#ffffff")
                        } else {
                            drawEyes(ctx, gx, gy, r)
                        }
                    }
                }

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.fillStyle = "#000000"
                    ctx.fillRect(0, 0, width, height)
                    const c = width / root.cols
                    const walls = game.mazeData
                    const items = game.itemData
                    const total = game.mazeCols * game.mazeRows
                    for (let i = 0; i < total; ++i) {
                        const x = i % game.mazeCols
                        const y = (i - x) / game.mazeCols
                        const wall = walls.charAt(i)
                        if (wall === "#") {
                            ctx.fillStyle = "#2121de"
                            ctx.fillRect(x * c + 1, y * c + 1, c - 2, c - 2)
                        } else if (wall === "-") {
                            ctx.fillStyle = "#ffb8de"
                            ctx.fillRect(x * c + c * 0.15, y * c + c * 0.4, c * 0.7, c * 0.2)
                        }
                        const item = items.charAt(i)
                        if (item === ".") {
                            circle(ctx, (x + 0.5) * c, (y + 0.5) * c, Math.max(c * 0.08, 1), "#ffb8ae")
                        } else if (item === "o") {
                            circle(ctx, (x + 0.5) * c, (y + 0.5) * c, Math.max(c * 0.25, 2), "#ffb8ae")
                        } else if (item === "F") {
                            circle(ctx, (x + 0.5) * c, (y + 0.5) * c, Math.max(c * 0.35, 3), "#ff3131")
                        }
                    }
                    drawPlayer(ctx, c)
                    drawGhosts(ctx, c)
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
                visible: game.paused
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
        case Qt.Key_Escape:
            game.togglePause()
            break
        case Qt.Key_R:
            game.reset()
            break
        default:
            return
        }
        event.accepted = true
        }
    }
}
