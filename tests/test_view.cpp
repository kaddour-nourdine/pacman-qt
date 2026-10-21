#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>

#include "AndroidBridge.hpp"
#include "AppSettings.hpp"
#include "GameController.hpp"
#include "ScreenInsets.hpp"
#include "SfxPlayer.hpp"

namespace {

// Finds the first item with the given `text` property (the menu entries).
QQuickItem* findItemByText(QQuickItem* root, const QString& text) {
    if (root->property("text").toString() == text) {
        return root;
    }
    const QList<QQuickItem*> children = root->childItems();
    for (QQuickItem* child : children) {
        if (QQuickItem* found = findItemByText(child, text)) {
            return found;
        }
    }
    return nullptr;
}

// Clicks the centre of an item (the entries carry their own MouseArea).
void clickItem(QQuickWindow* window, QQuickItem* item) {
    const QPointF centre = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, centre.toPoint());
    QTest::qWait(50);
}

}  // namespace

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
        ScreenInsets insets;
        AndroidBridge android;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("game", &controller);
        engine.rootContext()->setContextProperty("sfx", &sfx);
        engine.rootContext()->setContextProperty("insets", &insets);
        engine.rootContext()->setContextProperty("android", &android);
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

    void testBackKeyOpensThePauseMenu() {
        GameController controller;
        SfxPlayer sfx;
        ScreenInsets insets;
        AndroidBridge android;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("game", &controller);
        engine.rootContext()->setContextProperty("sfx", &sfx);
        engine.rootContext()->setContextProperty("insets", &insets);
        engine.rootContext()->setContextProperty("android", &android);
        engine.load(QUrl("qrc:/qt/qml/PacMan/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->requestActivate();
        QTest::qWait(50);

        QVERIFY(!window->property("menuOpen").toBool());

        // The Android back button and the Escape key open the same menu.
        QTest::keyClick(window, Qt::Key_Back);
        QTest::qWait(50);
        QVERIFY2(window->property("menuOpen").toBool(), "the back key did not open the pause menu");

        QTest::keyClick(window, Qt::Key_Back);
        QTest::qWait(50);
        QVERIFY(!window->property("menuOpen").toBool());
    }

    void testPauseMenuEntriesToggleTheSettings() {
        // The menu is the only way to reach the settings on a phone, so the
        // touch handling of its entries is worth a test.
        GameController controller;
        SfxPlayer sfx;
        ScreenInsets insets;
        AndroidBridge android;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("game", &controller);
        engine.rootContext()->setContextProperty("sfx", &sfx);
        engine.rootContext()->setContextProperty("insets", &insets);
        engine.rootContext()->setContextProperty("android", &android);
        engine.load(QUrl("qrc:/qt/qml/PacMan/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->requestActivate();
        QTest::qWait(50);

        const bool soundBefore = controller.settings()->soundEnabled();
        const bool slowBefore = controller.settings()->slowMode();

        QTest::keyClick(window, Qt::Key_Back);
        QTest::qWait(50);
        QVERIFY(window->property("menuOpen").toBool());

        // The entries show the state they switch.
        QQuickItem* soundEntry =
            findItemByText(window->contentItem(), soundBefore ? "SOUND: ON" : "SOUND: OFF");
        QVERIFY2(soundEntry != nullptr, "the sound entry is missing from the menu");
        QQuickItem* speedEntry =
            findItemByText(window->contentItem(), slowBefore ? "SPEED: SLOW" : "SPEED: NORMAL");
        QVERIFY2(speedEntry != nullptr, "the speed entry is missing from the menu");

        clickItem(window, soundEntry);
        QCOMPARE(controller.settings()->soundEnabled(), !soundBefore);

        clickItem(window, speedEntry);
        QCOMPARE(controller.settings()->slowMode(), !slowBefore);

        // Put the settings back where they were.
        controller.settings()->setSoundEnabled(soundBefore);
        controller.settings()->setSlowMode(slowBefore);
    }

    void testAndroidBackBridgeOpensThePauseMenu() {
        // What the Android activity does through JNI: it calls the bridge,
        // and the menu opens.
        GameController controller;
        SfxPlayer sfx;
        ScreenInsets insets;
        AndroidBridge android;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("game", &controller);
        engine.rootContext()->setContextProperty("sfx", &sfx);
        engine.rootContext()->setContextProperty("insets", &insets);
        engine.rootContext()->setContextProperty("android", &android);
        engine.load(QUrl("qrc:/qt/qml/PacMan/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->requestActivate();
        QTest::qWait(50);

        emit android.backPressed();
        QTest::qWait(50);
        QVERIFY2(window->property("menuOpen").toBool(),
                 "the Android back bridge did not open the pause menu");
    }

    void testSuspendingFreezesTheModel() {
        GameController controller;
        controller.start();
        QTest::qWait(60);

        controller.setSuspended(true);
        QVERIFY(controller.suspended());

        // While suspended the model must not advance: no tick, no move.
        const double x = controller.playerX();
        const int score = controller.score();
        QTest::qWait(120);
        QCOMPARE(controller.playerX(), x);
        QCOMPARE(controller.score(), score);

        // Coming back must not replay the time spent in the background.
        controller.setSuspended(false);
        QVERIFY(!controller.suspended());
        QTest::qWait(120);
        QVERIFY2(controller.playerX() != x, "the game did not resume");
    }

    void testSettingsArePersisted() {
        {
            AppSettings settings;
            QCOMPARE(settings.soundEnabled(), true);
            QCOMPARE(settings.slowMode(), false);
            settings.setSoundEnabled(false);
            settings.setSlowMode(true);
        }
        {
            // A fresh object reads the values back from the settings file.
            AppSettings settings;
            QCOMPARE(settings.soundEnabled(), false);
            QCOMPARE(settings.slowMode(), true);
            settings.setSoundEnabled(true);
            settings.setSlowMode(false);
        }
    }

    void testSlowModeScalesTheModel() {
        GameController controller;
        controller.start();
        QTest::qWait(60);

        const double normal = controller.playerX();
        QTest::qWait(120);
        const double normalDistance = std::abs(controller.playerX() - normal);
        QVERIFY(normalDistance > 0.0);

        controller.settings()->setSlowMode(true);
        QTest::qWait(20);
        const double slowStart = controller.playerX();
        QTest::qWait(120);
        const double slowDistance = std::abs(controller.playerX() - slowStart);
        QVERIFY2(slowDistance < normalDistance,
                 "slow mode did not slow the model down");
        controller.settings()->setSlowMode(false);
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
