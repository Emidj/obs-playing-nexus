# Assets and artwork — phase 14

`ArtworkLoader` is a Playing host component, separate from Core and the shared
renderer. Instantiate, request, cancel, clear, inspect and destroy it on the GUI
thread. Call it only for visible, playing consumers; inactive/paused/null
snapshots cancel the consumer without scheduling acquisition or delivery.

Each request replaces the previous generation for its consumer. Delivery is
always queued on the owner, including cache hits and missing images. The result
contains the original trackRevision and immutable shared ImageData. A different
track, profile or policy must issue a new request; detaching a source must cancel
its consumer. Core is never modified. Callback code may destroy the loader.

Ready artwork takes precedence. With fallback enabled, unknown/missing/pending/
failed metadata or unsuccessful cover acquisition can resolve the configured
profile. With fallback disabled no profile is loaded. A broken profile produces
an empty image result. No fabricated placeholder or Twitch numeric ID is used.

Valid absolute drive-path LocalFile references acquire PNG/JPEG/WebP or bounded
static SVG. Asset IDs matching `[A-Za-z0-9_-]+\.(png|svg)` resolve under the
explicit immutable absolute package root; canonical traversal outside it fails.
The source uses its OBS module data directory's `assets` subdirectory. No files
are fabricated or searched by PATH. HTTPS remains Unsupported and may fall back
to a supported configured profile. There is no network
client or interpretation of DJ-PC paths as receiver files. Drive paths may be
mapped drives; this component does not certify filesystem latency or locality.

Encoded data is limited to 4 MiB, dimensions to 4096 per axis and 4,194,304 total
pixels before decode. Resident images fit 1024 per axis, preserve aspect ratio
and use RGBA8888 premultiplied. Small images are not enlarged. One demand-driven
worker performs file I/O/decode. Its pool expiry is zero; no polling, watching or
product timer is introduced. Animated formats are not played by this loader.

SVG is a restricted static subset: at most 256 KiB, 1024 elements, 16384
characters per attribute, the same pixel/dimension limits, and an allowlist of
svg/g/defs/rect/circle/ellipse/line/polyline/polygon/path/text/tspan. DTD, entities,
processing instructions, href, event attributes, url(), external images, scripts,
use, gradients, filters, foreignObject and animation elements are rejected.
The validated document is rasterized once with Qt6Svg on the worker. Unsupported
documents return an empty outcome or configured fallback, never a live SVG in
video_render. This is not unrestricted browser SVG compatibility.

At most eight acquisition jobs and 128 consumers are admitted. Simultaneous
requests for the same kind/location/version share one job and image. Capacity
is an explicit result, not an unbounded queue or retry loop. Completed results
(including failures) occupy an LRU of 32 entries/8 MiB by default. Configuration
is capped at 128 entries/32 MiB. Retained publications and decoder temporaries
are outside the cache budget; it is not a process memory measurement.

Reference versions are immutable cache keys. No file-stat polling or automatic
retry occurs for a failed reference. Change its version or clear the cache to
retry. Different consumer policies do not change the shared DJ Request/session.

Cancellation drops delivery, not an in-progress filesystem operation. Already
queued jobs may finish and populate the bounded cache. `clear` additionally
changes an epoch: old completions cannot repopulate it. Old jobs count towards
the admission limit until drained. New same-key demand waits for that obsolete
job to drain, then reads afresh under the new epoch, without a timer or accepting
old pixels. Different-key overload produces Capacity without autonomous retries.

Destruction waits for outstanding workers, while keeping the QObject completion
context alive. Queued deliveries are removed with the context. File operations
are bounded by input size, not by a certified wall-clock deadline; shutdown can
wait for the filesystem. Release all externally retained frames/images before
module unload. Reopening a source creates fresh consumer demand.

SessionHost owns one loader, cancels by consumer on hide/detach/stop, and prepares
an ImageNode on GUI completion. BASIC/NORMAL publish immutable image frames and
positions to the same shared renderer. Aspect ratio is preserved inside the
configured rectangle; a narrower image leaves transparent margins. Profile and
fallback changes keep the reader/Core/TLS session. Default artwork visibility is
off to preserve existing designs. The OBS profile reload button creates a fresh
local cache version without polling; its version survives unrelated settings
updates within that source's lifetime.

The source does not invent artwork metadata that current providers do not
expose. Acquisition can render verified Ready references, while existing readers
without references use the receiver's optional configured streamer profile.
Remote local paths/Asset IDs remain Missing by the codec's existing contract.
HTTPS fetching, remote binary image transfer, physical two-PC artwork testing,
new DJ extraction mappings and final packaging are outside this development
checkpoint. No CPU/GPU/energy or Idle≈Zero certification is implied by budgets.
