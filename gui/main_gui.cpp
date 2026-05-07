#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QLocale>
#include <QDir>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("w3x-packer");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("w3x");

    // Load system Qt translations (e.g. qt_zh_CN.qm)
    QTranslator qtTranslator;
    if (qtTranslator.load("qt_" + QLocale::system().name(),
        QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // Load app translations based on system locale
    QTranslator appTranslator;
    QString transDir = QApplication::applicationDirPath() + "/translations/";
    QString lang = QLocale::system().name(); // e.g. "zh_CN"
    bool loaded = appTranslator.load("w3x-packer_" + lang, transDir)
               || appTranslator.load("w3x-packer_" + lang.left(2), transDir);
    if (loaded)
        app.installTranslator(&appTranslator);

    MainWindow window;
    window.resize(1024, 700);
    window.show();

    return app.exec();
}
