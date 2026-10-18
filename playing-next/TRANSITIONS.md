# Track transitions (phase 16)

The GUI-owned Timeline coordinates OUT, an invisible midpoint swap, and IN.
Fade, Slide and Zoom last 200–2000 ms total, with equal OUT/IN halves. First
publication and return from hidden state show the latest song directly. Playing
changes transition; Paused, Waiting, Unavailable, expiry and stop clear active
presentation immediately. Reconfiguration cancels an unfinished transition and
applies the latest song without restarting the Core/provider/TLS session.

BASIC transitions the complete text/artwork presentation. NORMAL retains its
existing title/artist transition by default; `transition_whole=true` opts into
whole-card transitions including label, background, decorations and artwork.
The OBS property is "Transicion de toda la tarjeta (incluye portada)". Existing
23 NORMAL settings and 3 aliases keep their mapping. Duration/style are available
for BASIC as well. This is a per-consumer policy saved in ordinary OBS settings.

Title/artist differences trigger a transition. Two known authoritative identities
also trigger one when scope/value differ, even with identical title/artist.
Missing identity is not interpreted as a different song. Artwork-only enrichment
does not replay OUT/IN. There is no identity invented from a title or pointer.

The displayed base retains its real snapshot and CPU frames through OUT. Changes
during OUT replace a single pending slot; the latest available song wins the
midpoint. Changes during IN wait for IN to finish, then start the next OUT. These
slots are bounded and do not queue every intermediate observation. Core continues
to expose its latest snapshot independently of the presentation.

Artwork follows the displayed snapshot. At swap, the old image is removed and
the selected snapshot requests its cover/profile under the existing loader's
consumer generation. A late callback may enrich the outgoing presentation while
it is still displayed, but cannot attach its image after the selected snapshot
changes. The transition never waits for disk I/O: IN may initially have no image;
the matching image joins at the current IN opacity when available. Fallback is
the configured local streamer profile, independently for each consumer.

The animation clock from phase 15 remains independent. Intrinsic layer motion
continues under the outer transition; hiding freezes it, reconfiguration resets
it. All samples are immutable GUI publications. The one existing animation timer
is active only for transition/decorations/overflow/layer motion demand. Static
BASIC/Prism/Air return to no animation timer after IN. A decorated NORMAL can
still need that timer for its own movement.

Whole-card GPU composition calls the same `drawContent`/shared renderer used for
direct rendering, into one reusable RGBA target per module renderer host. Its
opacity/scale/translation is applied once to the complete target, avoiding double
text fades and alpha multiplication at overlapping layer edges. Classic GDI
StraightSrgb inputs remain on their existing draw path: the shared encoded group
API rejects that encoding, so it is not used or modified. No second preset
renderer or external engine edit is introduced. Default NORMAL uses direct
rendering throughout; whole mode and BASIC use the target only during transition.

Render performs no layout, image decoding, disk/network I/O, Core mutation, clock
sampling or CPU rasterization. GPU target/effect creation is lazy, then reused;
the target is composed for each rendered transition sample. One target is at
most 2048×2048 RGBA, 16 MiB texture payload, in addition to the shared renderer's
64 MiB texture budget. This bound excludes driver/OBS overhead and is not a
measurement of process memory. The target/effect are released together with the
shared renderer when the last active source leaves or before OBS graphics dies.
All access follows graphics-context → renderer-mutex order.

Tests distinguish actual-clock source flow from diagnostic fixed samples. The
former exercises swap/coalescing/hide/resume through private OBS sources. Fixed
samples exercise the exact draw path with frozen intrinsic motion: direct vs.
full-opacity identity, half-opacity alpha, invisible midpoint, slide and zoom.
No diagnostic export is present in the final hooks-OFF build. These fixtures do
not certify LAN hardware, arbitrary OBS versions, HDR/OpenGL, third-party plugins,
real DJ artwork extraction or CPU/GPU/Idle≈Zero targets.
