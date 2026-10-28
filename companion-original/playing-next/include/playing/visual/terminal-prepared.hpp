// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "authored-text.hpp"
#include "terminal-layout.hpp"
namespace playing::visual::normal {
// Text/geometry preparation only. Full scene parity remains a separate gate.
struct PreparedTerminalText {
    TrackPtr track;
    TerminalLayout layout;
    std::array<PreparedAuthoredText,3> rows;
    PreparedAuthoredText onAir,prompt;
};
using PreparedTerminalTextPtr=std::shared_ptr<const PreparedTerminalText>;
PreparedTerminalTextPtr prepareTerminalText(oto::visual::VisualEngine &,TrackPtr,
    int width,int gap,const QString &label,const AuthoredGlyphRaster &);
}
