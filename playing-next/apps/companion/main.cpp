// SPDX-License-Identifier: MIT
#include "window.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setApplicationName("OBS Playing Companion"); app.setOrganizationName("OBS Playing Next");
    app.setApplicationVersion("1.0.0-rc3.4"); app.setWindowIcon(QIcon(":/branding/nexus-app.ico"));
    QCommandLineParser parser; parser.addHelpOption(); parser.addOption({"profile-dir","Directorio propio del perfil protegido","path"}); parser.addOption({"db-library","Dependencia DB auditada","path"}); parser.process(app);
    try {
        const auto profileDir=parser.isSet("profile-dir")?parser.value("profile-dir"):QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if(!QFileInfo(profileDir).isAbsolute() || !QDir().mkpath(profileDir)) throw std::runtime_error("Profile directory unavailable");
        const auto library=parser.isSet("db-library")?parser.value("db-library"):QDir(QCoreApplication::applicationDirPath()).filePath("obs-playing-db.dll");
        QFile file(library); if(!QFileInfo(library).isAbsolute() || !file.open(QIODevice::ReadOnly) || QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex()!="5030decc6d914539e3b9b7e28aa4f6de1e7161dac6fd4b21eb02e6754d9b175e") throw std::runtime_error("Audited dependency unavailable");
        playing::companion::Window window(QDir(profileDir).filePath("profile.pcs"),QFileInfo(library).absoluteFilePath().toUtf8().toStdString()); window.show(); return app.exec();
    } catch(const std::exception &) {QMessageBox::critical(nullptr,"OBS Playing Companion","No se pudo abrir el Companion. Comprueba su carpeta de instalación y el perfil protegido."); return 1;}
}
