#include "QtGui/qsurfaceformat.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QScreen>

#include "parser/sceneparser.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Set OpenGL version to 4.1 and context to Core
    QSurfaceFormat fmt;
    fmt.setVersion(4, 1);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    MainWindow w;
    w.show();

    SceneParser::debugDFS();

    return a.exec();
}
