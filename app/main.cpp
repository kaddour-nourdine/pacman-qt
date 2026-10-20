#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>

#include <atomic>

#include "GameController.hpp"
#include "SfxPlayer.hpp"

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("pacman-qt"));
    QCoreApplication::setApplicationName(QStringLiteral("pacman"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/pacman.svg")));

    GameController controller;
    SfxPlayer sfx;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("game"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("sfx"), &sfx);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [](const QUrl& object) {
                         qWarning() << "pacman: failed to create QML root object" << object;
                         QCoreApplication::exit(-1);
                     },
                     Qt::DirectConnection);
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError>& warnings) {
        for (const QQmlError& error : warnings) {
            qWarning() << "pacman: QML warning:" << error.toString();
        }
    });
    engine.loadFromModule("PacMan", "Main");
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "pacman: QML root object was not created"
                   << "(platform:" << QGuiApplication::platformName() << ")";
        return -1;
    }

    // Render-thread cadence probe (diagnostic, part of the frame-gap
    // investigation): fires on the scene-graph thread, so it sees whether
    // that thread keeps rendering while the UI timer stalls. Atomic because
    // it runs on the render thread; only logs when frames stop for >40 ms.
    if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst())) {
        static std::atomic<qint64> lastRenderMs{0};
        QObject::connect(window, &QQuickWindow::afterRendering, &app, [] {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 prev = lastRenderMs.exchange(now);
            if (prev > 0 && now - prev > 40) {
                qInfo() << "pacman-perf: render-thread gap" << (now - prev) << "ms";
            }
        }, Qt::DirectConnection);
    }

    return app.exec();
}
