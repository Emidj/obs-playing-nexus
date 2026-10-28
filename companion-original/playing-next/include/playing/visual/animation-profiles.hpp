// SPDX-License-Identifier: MIT
#pragma once
#include "layer-animation.hpp"
#include <QByteArray>
#include <QString>
namespace playing::visual::animation {
struct Profile {int setting=0;QString name;Program program;};
std::vector<Profile> parseProfiles(const QByteArray &bytes);
const std::vector<Profile>& profiles();
const Program &program(int setting);
}
