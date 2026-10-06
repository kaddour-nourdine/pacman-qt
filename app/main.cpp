#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>

#include "GameController.hpp"

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("pacman-qt"));
    QCoreApplication::setApplicationName(QStringLiteral("pacman"));

    GameController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("game"), &controller);
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

    return app.exec();
}
