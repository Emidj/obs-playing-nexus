// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <functional>
class QDialog;
class QWidget;
namespace playing::presets {
struct Entry { QString id,name,base; int group=0; QJsonObject appearance; };
const QVector<Entry>& builtins();
QStringList visualKeys();
QJsonObject capture(const QJsonObject& settings);
void validate(const QJsonObject& appearance);
QByteArray encodePreset(const Entry&);
Entry decodePreset(const QByteArray&);
class Library {
public:
    explicit Library(QString path);
    void reload();
    QVector<Entry> entries() const;
    bool favorite(const QString& id) const;
    void toggleFavorite(const QString& id);
    QString saveAs(const QString& name,const QJsonObject& appearance);
    void replace(const QString& id,const QJsonObject& appearance);
    void rename(const QString& id,const QString& name);
    void remove(const QString& id);
    QString importFile(const QString& path);
    void exportFile(const QString& id,const QString& path);
private:
    QString path_; QJsonObject document_;
    void mutate(const std::function<void(QJsonObject&)>& operation);
};
QDialog* showManager(const QString& path,std::function<QJsonObject()> current,
    std::function<void(const Entry&)> apply,QWidget* parent);
}
