#include "app/AppController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("BLOCK SYSTEM");
    QGuiApplication::setOrganizationDomain("blocksystem.nl");
    QGuiApplication::setApplicationName("beater");
    QQuickStyle::setStyle("Basic");

    AppController controller;

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"app", QVariant::fromValue(&controller)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Beater", "Main");
    return app.exec();
}
