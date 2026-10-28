// SPDX-License-Identifier: MIT
#pragma once
#include <QString>
#include <functional>
class QWidget;class QDialog;
struct PlayingReceiverHealth {bool connected=false,track=false,image=false;QString message;};
QDialog *showPlayingReceiverLink(const QString &profilePath,std::function<void()> changed,QWidget *parent,
    std::function<PlayingReceiverHealth()> health={},bool automaticSearch=true);
