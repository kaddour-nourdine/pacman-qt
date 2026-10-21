#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>

#include "GameController.hpp"
#include "SfxPlayer.hpp"

class ViewTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Keep materializeBundledLevels() out of the real app data directory.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("pacman-qt"));
        QCoreApplication::setApplicationName(QStringLiteral("pacman"));
    }

    void testBundledLevelsAreShipped() {
        QVERIFY2(QFile::exists(QStringLiteral(":/levels/level1.txt")),
                 "levels/*.txt must be embedded as Qt resources for Android/install builds");

        const QString dir = GameController::materializeBundledLevels();
        QVERIFY2(!dir.isEmpty(), "materializing the bundled levels failed");
        for (const char* name : {"level1.txt", "level2.txt", "level3.txt"}) {
            QVERIFY2(QFile::exists(dir + QLatin1Char('/') + name), name);
        }

        pac::LevelManager manager;
        std::string error;
        QVERIFY2(manager.loadDirectory(dir.toStdString(), &error), error.c_str());
        QCOMPARE(manager.levelCount(), 3);
    }

    void testLoadAndInput() {
        GameController controller;
        SfxPlayer sfx;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("game", &controller);
        engine.rootContext()->setContextProperty("sfx", &sfx);
        engine.load(QUrl("qrc:/qt/qml/PacMan/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);

        window->requestActivate();
        QTest::qWait(50);

        controller.start();

        QTest::keyClick(window, Qt::Key_Right);
        QTest::qWait(50);

        QCOMPARE(static_cast<int>(controller.directionForTest()), 2);
    }

    void testMixerFeedsTheAudioSink() {
        // Regression guard. Qt's audio pipeline only pulls from a user
        // QIODevice when it reports data (pullFromQIODeviceToRingbuffer()
        // gives up when bytesAvailable() == 0). A plain QIODevice reports 0
        // forever, so the sink idled without ever reading a frame and the
        // game was silent - found on the phone, invisible on the desktop.
        SfxPlayer sfx;
        if (!sfx.hasSink()) {
            QSKIP("no audio output device on this machine");
        }

        sfx.playMunch();
        QCOMPARE(sfx.voicesStarted(), 1);

        // The sink pulls on its own thread; give it a few callbacks.
        QTest::qWait(500);

        QVERIFY2(sfx.framesWritten() > 0,
                 "the audio sink never read a single frame from the mixer");
        QVERIFY2(sfx.peakLevel() > 0.0f, "the mixer produced silence only");
    }
};

QTEST_MAIN(ViewTest)

#include "test_view.moc"
