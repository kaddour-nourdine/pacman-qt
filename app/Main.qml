import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtMultimedia

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
                    Image {
                        source: "qrc:/assets/sprites/life.svg"
                        width: 20
                        height: 20
                        fillMode: Image.PreserveAspectFit
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
                    case 4: base = Math.PI; break
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

        SoundEffect {
            id: munchSound
            source: "qrc:/assets/audio/munch.wav"
            volume: 0.6
        }

        SoundEffect {
            id: powerSound
            source: "qrc:/assets/audio/power.wav"
            volume: 0.6
        }

        Connections {
            target: game
            function onPelletEaten(x, y) {
                munchSound.play()
            }
            function onPowerPelletEaten(x, y) {
                powerSound.play()
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
