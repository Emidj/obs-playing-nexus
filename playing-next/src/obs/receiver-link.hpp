// SPDX-License-Identifier: MIT
#pragma once
#include <QString>
#include <functional>
class QWidget;class QDialog;
QDialog *showPlayingReceiverLink(const QString &profilePath,std::function<void()> changed,QWidget *parent);
