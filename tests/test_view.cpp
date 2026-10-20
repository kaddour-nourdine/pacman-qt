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
};

QTEST_MAIN(ViewTest)

#include "test_view.moc"
