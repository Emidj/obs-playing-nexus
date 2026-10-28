# Layer animation — phase 15

`layer-animation.hpp` is a pure C++17 sampler: no Qt, clock acquisition, I/O,
Core mutation, renderer ownership or timers. Immutable validated Clip data
contains 2..32 normalized keys, duration 200..60000 ms, delay 0..60000 ms and
Once/Repeat/PingPong. Repeat requires matching first/last values; PingPong has
a full cycle of twice the duration. Linear/Cosine/Step easing belongs to the
outgoing key. Once holds its final value and stops requesting animation frames.
Constant clips do not request frames. Sampling rejects negative/nonfinite time.

Values: translation x/y (-4096..4096 pixels), opacity 0..1, uniform scale
.25..4, rotation -360..360 degrees. There is no expression evaluator, recursion,
script or dynamic allocation in sample. Four logical slots are Label, Title,
Artist and Artwork. The host adapter rejects text rotation because the real
shared renderer rejects rotation combined with its clipping rectangle. Artwork
rotation is supported. This is not an arbitrary editable layer graph.

`animations-v1.json` is an embedded, versioned, immutable profile resource.
Parser limits: 64 KiB, 16 profiles, four unique targets per profile and 32 keys
per target. Unknown fields, duplicate numeric settings/targets, invalid values,
open repeat seams and nonempty default setting0 fail validation. The module
preflights the catalogue before source registration. Qt JSON's duplicate object
keys are last-wins; this is a trusted resource, not an external import endpoint.
Changing recipes requires rebuilding. Editor/import/hot reload remain later work.

The source setting `layer_animation` is independent of DJ Request. Setting0 is
"Movimiento del preset", retaining the previous NORMAL animation; it does not
turn off existing ornament motion or scrolling. Settings1/2 add subtle floating
or breathing to logical layers. Labels apply to NORMAL, title/artist to both
BASIC/NORMAL and artwork only when present. IDs/presets/aliases remain unchanged.

SessionHost evaluates additional layer samples on GUI from one monotonic Clock
per consumer. The existing single animation QTimer serves all consumers; no
timer or worker is created per layer. Hidden/paused/suppressed consumers freeze
their phase. Resume continues it, while changing visual configuration resets it.
Track changes alone do not reset this intrinsic clock; OUT/IN orchestration is
phase16. Unchanged samples do not republish frames, and finished/static programs
alone cannot keep the animation timer alive. Existing decoration/overflow can.

Samples are plain values in immutable TrackFrame publications. Render consumes
them through the existing renderer; it does not parse JSON, read clocks, perform
I/O, shape text or rasterize. Legacy ornament/quad oscillation reuses the common
pure wave helper, preserving recipe parameters and timing. Animation does not
create a renderer, texture or Core per layer.

Text translation moves its clipping viewport as well as the content. Existing
scroll/track-transition clipping remains relative to that moved viewport. NORMAL
text scales around its row geometry; BASIC/image transforms use the renderer's
origin. Combined NORMAL zoom is capped at the renderer's scale limit4. Parent
canvas still clips content; no auto-height or pivot editor was added.

Observed stable raster/upload counters in fixtures demonstrate no new prepares
or uploads during those motion intervals. They are not CPU/GPU usage measurements
or a global Idle≈Zero guarantee. Local provider refresh can remain active while
the source is active; only the animation timer's demand was checked here.
