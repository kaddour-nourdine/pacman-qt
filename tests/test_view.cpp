#include <QtTest/QtTest>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

#include "GameController.hpp"

class ViewTest : public QObject {
    Q_OBJECT
private slots:
    void testLoadAndInput() {
        GameController controller;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("game", &controller);
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
