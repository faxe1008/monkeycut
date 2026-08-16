# MonkeyCut — Design Brief

A frame-accurate, stream-copy video cutter for advertised TV recordings
(MPEG-TS/PS/AVI/MP4), with first-class integration of the
[cutlist.at](http://cutlist.at/#!/) (Sniplist) cutlist database.
Built with Qt/C++, packaged portable for **Linux** and **Windows**.

Heavily inspired by *cutana* (now offline): open recording → verify/adjust
cuts → strip ads → save, with a preview window and frame stepping.

---

## 1. Goals / non-goals

### Goals
1. **No re-encoding, format preserved.** Output is an exact stream-copy
   of the source streams, and the **output container matches the input**
   (AVI → AVI, MPEG-TS → TS, …). Export speed limited only by disk I/O.
2. **Frame-accurate positioning.** Step ±1 frame while previewing, jump to
   any cut boundary, verify visually before cutting.
3. **cutlist.at workflow in seconds**: search an episode → download cutlist
   → apply to the local recording → verify a few cut points → export.
4. **CUL cutlist files** (the format cutlist.at distributes) can be
   imported, edited and exported, so cutlist.at data is usable offline and
   interchangeably with other tools (ColdCut/VirtualDub ecosystem).
5. **Portable**: single folder/AppImage on Linux, single folder on Windows.
   No system-wide installs, no admin rights.

### Non-goals (v1)
- Re-encoding: **out of v1**. A disabled "Quality mode" entry is stubbed
  in Settings as the placeholder for the v2 re-encode option.
- Container *conversion* (AVI→MP4 etc.): not a feature, only an advanced
  override in the export dialog (§6.4).
- Audio video editing, filters, subtitles, multi-track, transitions.
- cutlist.at account features (login, upload, rating, shoutbox) — the
  client leaves the hook (Bearer token) but does not implement flows.
- Linux/macOS ARM, video decoding via GPU (v2: d3d11va/vaapi).

---

## 2. Research findings: cutlist.at

### 2.1 HTTP API (plain **http**, no TLS — accept as-is, base URL configurable)

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/get-info` | GET | `{name, version}` — connectivity check |
| `/api/search-by` | POST JSON | Search cutlists |
| `/getfile.php?id=<id>` | GET | Download CUL file |
| `/getfile.php?raw=1&nohit=1&id=<id>` | GET | Same, without inflating hit counter (use this) |

`POST /api/search-by` request:

```json
{"conds":[{"query":"Die_Simp","field":"name"}],
 "isOrConnection":false,"sortBy":"date","isAsc":false,"page":0}
```

- `conds[].field` — at least `name`; assume `channel`, `author`, `otrkey`
  work too (verify in M4, fall back to `name`).
- `isOrConnection` — AND vs OR between conditions.
- `sortBy` — `date` observed; treat others as bonus.
- Response: `{"items":[...], "hasMore":true, "currentPage":0}` —
  pagination by incrementing `page` while `hasMore`.

Item schema (observed):

```
id            int     2077248
name          str     "Die Simpsons  Mission Simpossible"
airDate       ISO8601 2026-05-29T05:05:00+02:00
uploadDate    ISO8601
otrkey        str     original recording filename:
                      "Die_Simpsons__..._orf1_20_TVOON_DE.mpg.HD.avi"
comment       str     "Mit ColdCut geschnitten"
suggestedName str     "Die_Simpsons_S30E14_..."
channel       str     "orf1", "pro7", ...
author        str
rating        {avg, avgRounded, ratings, fillPercent, author}
registeredDownloads str
hits          int
duration      "HH:MM:SS"
quality       "hd" | "hq"
cutCount      int
errors        {start,end,video,audio,other,epg, otherDesc}
_my.canRate   bool
```

Auth: optional `X-Authorization: Bearer <token>` header for
account-bound endpoints — not needed for search/download.

### 2.2 The CUL file (what `getfile.php` returns)

INI text, ColdCut/VirtualDub-compat, **keep-segment semantics**:
*"the following parts of the movie will be kept, the rest will be cut out"*.

```ini
[General]
Application=ColdCut
Version=1.0.8.6
FramesPerSecond=25
DisplayAspectRatio=16:9
IntendedCutApplicationName=VirtualDub
comment1=The following parts of the movie will be kept, ...
NoOfCuts=2
ApplyToFile=Die_Simpsons__Zu_Ehren_von_Murphy_26.03.28_16-10_pro7_30_TVOON_DE.mpg.HQ.avi
OriginalFileSizeBytes=405016566

[Cut0]
Start=437.96
StartFrame=10949
Duration=615.04
DurationFrames=15376

[Cut1]
Start=1490.24
StartFrame=37256
Duration=678.52
DurationFrames=16963

[Info]
Author=b-andi-t
RatingByAuthor=5
SuggestedMovieName=Die_Simpsons_S06E22_Zu_Ehren_von_Murphy
UserComment=Mit ColdCut geschnitten
```

Notes:
- `Start`/`Duration` are **seconds with dot decimal** (locale-neutral
  parse), `StartFrame`/`DurationFrames` integer frames at
  `FramesPerSecond`. Trust frames when both present; recompute otherwise.
- `ApplyToFile` is the *expected source filename* → use for "smart apply"
  (match against user's local files) and for sanity warnings.
- `MissingBeginning/MissingEnding` (in `[Info]` of some files) mean the
  cutlist creator did not have the full episode → the resulting file will
  start/end mid-content. Surface as a warning.
- We can write the same format → our exports of a cutlist are consumable
  by ColdCut/VirtualDub users, and we stay in the same ecosystem.

### 2.3 Typical recording files (from `otrkey`)

`..._pro7_25_TVOON_DE.mpg.HQ.avi` → MPEG-2 video + MP2 audio in AVI,
25fps, plus `.mpg`/`.ts`/`.m2ts` variants. 50fps files also observed
(`FramesPerSecond=50`).

---

## 3. The core technical truth (read this first)

**Frame-accurate cutting without re-encoding is impossible for the video
stream.** A video stream copy can only start at a **keyframe (GOP start)**;
anything between keyframes is undecodable on its own.

Consequences, designed around rather than away:

1. The user positions cuts at frame granularity (that's what frame stepping
   is for). The cutlist model stores the *intended* frame-accurate
   boundaries.
2. The engine computes the *effective* boundary at cut time:
   video start snaps **back** to the previous keyframe (≤ 1 GOP of the
   prior scene may remain — invisible for ad cuts; the cutlist creator's
   timestamps already assume this), video end is a free boundary (the
   *next* kept segment snaps forward).
3. The UI always shows **both**: the frame you picked and the frame the
   cut will actually land on (with an indicator how far they are).
4. **Audio is trimmed sample-accurate** (frames ≈ 23 ms for AAC, MP2 is
   exact) and aligned to the snapped video start.
5. For the primary use case (removing ads between program segments of
   25/50 fps MPEG-TS/PS/AVI recordings, GOPs typically ≤ 1 s) the result
   is visually identical to a frame-accurate cut.

We are explicitly *not* a re-encoder; a re-encode "quality mode" can be a
v2 opt-in.

---

## 4. Architecture

```
┌────────────────────────────────────────────────────────────┐
│ UI (Qt Widgets, QML optional later)                        │
│  MainWindow / VideoView / CutTable / Timeline / dialogs    │
└──────────▲─────────────────────────────▲───────────────────┘
           │ signals/slots               │
┌──────────┴──────────────┐   ┌──────────┴───────────────────┐
│ Net: CutlistAtClient    │   │ Core (pure C++, no Qt)       │
│  QNetworkAccessManager  │   │  Time, TimeCode, Fps         │
│  search / getinfo       │   │  Cutlist, Cut, CutlistMeta   │
│  download(→QByteArray)  │   │  CulFile (parse/serialize)   │
│  (upload slot: v2)      │   │  CutPlanner (keep↔remove)    │
└──────────┬──────────────┘   │  GopMap (keyframe index)     │
           │                   │  Project (model, undo)      │
           ▼                   └──────────▲───────────────────┘
┌──────────┴──────────────────────────────┐ │
│ AV (FFmpeg libav* wrappers)             │ │
│  AvProbe        → MediaInfo             │ │
│  GopScanner     → GopMap (bg thread)    │ │
│  Player         → frames + audio        │ │
│  CuttingEngine  → AVStreamCopy output   │
└─────────────────────────────────────────┘
```

Threading:
- **AV threads** never touch Qt widgets directly; all results via
  signals (frame callback → `requestUpdate` + texture buffer,
  progress → progress bar).
- `GopScanner` and export run in `QThreadPool`/`std::thread` with
  cooperative cancellation (`std::atomic<bool> stop`).
- `CutlistAtClient` on the GUI thread with `QNetworkAccessManager`
  (async, small payloads).

FFmpeg: link **libraries** (`avformat`, `avcodec`, `avutil`, `swscale`,
`swresample`, `avdevice` not needed) — no CLI process at runtime.
Same static/shared build story on both OSes, keeps the package clean.

---

## 5. Data model (core, Qt-independent)

```cpp
struct Time {              // exact, no rounding loss
  Rational tb;             // per-stream timebase or fps-based
  qint64  ticks;
};
struct TimeCode { Time t; double fps; /* render HH:MM:SS:FF (ms + frames) */ };

struct Fps { int num, den; }                       // 25/1, 50/1, 30000/1001...

struct Cut {                     // one segment, indices in *source video frames*
  int64_t startFrame, endFrame;  // [start, end)
  QString note;                  // e.g. "Ad: Xinge" / "ColdCut seg 1"
  bool    verified = false;
};

struct Cutlist {
  QString sourceHint;            // ApplyToFile / otrkey
  int64_t sourceFileSize = 0;
  Fps     fps;                   // from CUL or from probe (probe wins)
  QList<Cut> keep;               // CUL semantics: these segments are KEPT
  struct { QString author, suggestedName, comment;
           bool missingBegin=false, missingEnd=false;
           bool epgError=false; QString errorDesc; } meta;
};

struct GopMap {
  QVector<int64_t> keyframes;    // frame index of each keyframe
  int64_t snapBack(int64_t f) const;  // greatest keyframe <= f
  int64_t snapFwd (int64_t f) const;  // smallest keyframe >= f
};
```

Semantics helpers in `CutPlanner`:
- `keepSegments(cuts) → QList<OutputSegment>` where
  `OutputSegment { videoStartFrame = gop.snapBack(cut.start),
                    videoEndFrame = cut.end,
                    audioStart = time(cut), audioEnd = time(cut.end) }`
  — first kept segment: if `cut0.start > 0` the file starts at the
  snapped keyframe of cut0 (program head).
- `removedSegments(keep)` → the "cut out" view for the UI (complement of
  keep ∪ [0, first.start) ∪ (last.end, duration]).
- Validation: overlaps, zero-length, out-of-range, fps mismatch CUL vs
  probe (warn, offer "trust CUL" / "trust file" / re-map by frame index
  scaling).

Persistence: native project file **JSON** (`.mproject`) = cutlist +
source path + gop cache reference + per-cut verified flags. CUL
import/export is the *exchange* format (cutlist.at compatibility), not
the project format.

---

## 6. AV layer (FFmpeg)

### 6.1 Probe (`AvProbe`)
`avformat_open` → for the chosen video/audio streams report:
codec name/ID, duration (format-level + per-stream), fps (r_frame_rate,
fall back to avg_frame_rate and *detect actual* via first 100 packets'
deltas — **VFR: allowed but warned** "frame stepping uses decode order,
cut accuracy limited"; affected files get a persistent badge and their
cuts are tagged *accuracy-limited* in the cut table), resolution,
SAR/DAR, channel layout/sample rate, packet count, seekability,
container type.

**Stream selection rule (owner decision)**: map the *first* video and
*first* audio stream. When the file carries more (stereo + 5.1, two
languages, …) show a warning in the status bar — no track-picker UI in v1.

### 6.2 GOP scan (`GopScanner`)
Single sequential demux pass (no decoding — reads packed headers only:
fast, even multi-GB in seconds):
`while ((pkt = av_read_frame)) if (pkt.stream_index==v && AV_PKT_FLAG_KEY) keyframes.push_back(frameCounter)`.
`frameCounter` = number of video packets emitted so far (decode-order
index). Cache result next to the video as `<file>.mcut.json`
(hash + size + mtime validated), or in app data dir on read-only
volumes. Invalidate on video file change.

### 6.3 Player (`Player`)
Decode path: `avformat_open_input → find streams → avcodec open →
decode loop` with:
- **Texture pipeline**: `swscale` → NV12/RGBA → `QOpenGLWidget`
  2D-texture update (double buffered, one VBO per frame). Fallback to
  `QVideoWidget`-like `QImage` path if no OpenGL.
- **Audio path**: `swresample` → 48 kHz s16 stereo interleaved →
  `QAudioOutput` ring buffer. (Works on PulseAudio/ALSA and WASAPI
  shared mode without extra deps.)
- **Clock**: video-presentation clock driven by audio if present;
  seeking while playing: cancel decode, seek, resync.

- **Seek** two flavors:
  - `seekFrame(f, exact=true)` — `av_seek_frame` to keyframe ≤ f,
    decode+discard until frame f → display f. Used by "go to cut".
      (Discards up to 1 GOP of decode time; fine for 25–50 fps TV.)
  - `seekFrame(f, exact=false)` — land on keyframe (fast scrub).
- **Step ±1 frame**: decode exactly one (or rewind to last kept frame).
  Keep a small decode-order LIFO (≤ GOP size) for cheap step-back;
  deeper step-back re-seeks to previous keyframe.
- **Speed**: 0.5×/1×/2×/4× via audio-pull / drop-frame, for quickly
  scanning to an ad.
- **Preview-cut mode** (the money feature): "Play through cut N" —
  plays kept segments consecutively skipping removed ones, so the user
  can watch the *result* before exporting. For stream-copy preview this
  is just seek jumps; cheap.

### 6.4 Cutting engine (`CuttingEngine`) — stream copy, single pass

For each `OutputSegment` in order, over *one* demux pass of the source:

```
for each segment s (index i):
  compute shift = sum of kept durations of segments < i
  while av_read_frame():
    if pkt is in a removed zone      → drop
    if pkt is in segment s:
      video: start emitting from s.videoStartFrame (keyframe-snapped,
             all packets from the snap keyframe belong to s)
      audio: start emitting from s.audioStart (sample-accurate)
      pkt.pts/dts = (orig - segOrigStart) + shift   (per-stream timebase)
      → av_interleaved_write_frame(out, pkt)
  at segment boundary: flush nothing (stateless copy), handle
  discontinuities: for TS/PS simply continue; for MP4 set
  `-movflags`-equivalent on the muxer, insert edit lists on the first
  stream-copy segment only.
```

Container policy — **match the input format** (owner decision: don't
mess with the file's format; AVI in → AVI out, TS in → TS out).
Output extension derives from the *input* container, not a global
default:

| Input | Output | Stream-copy notes |
|---|---|---|
| .ts | .ts | ideal for stream copy; players tolerate cuts |
| .mpg / .m2ts | .mpg / .m2ts | keep original PS flavor; cuts land on packet boundaries |
| .avi | .avi | AVI muxer writes a proper index; keep the container the user expects. Stream-copy only (AVI's index is rebuilt automatically by libavformat) |
| .mp4 | .mp4 | edit lists + moov; verify keyframe alignment warnings |
| .mkv / .mov | .mkv / .mov | best-effort, same keyframe rules |

The output container is chosen from the **detected input codec/container**,
so the file stays in the format the user recorded it in. This is a
*hard default*; the export dialog exposes a manual override as an
**advanced** option only.

Audio alignment: audio window = the exact cut window, but the *first*
emitted video frame is the snap keyframe, so we also drop audio before
video start (standard practice). Result: A/V consistent within < 1 frame.

Audio cut precision is **codec-bounded, not sample-accurate**, and the UI
must not promise more:
- MP2 (typical in these recordings): frame = 1152 samples ≈ 23–24 ms →
  cut lands on the MP2 frame at the boundary.
- AAC: 1024 samples ≈ 23 ms, same order.
- **AVI caveat**: audio lives in `00cb` chunks (up to 512 kB ≈ 2–3 s of
  MP2). A cut inside a chunk cannot split it, so the audio join in an
  AVI output can carry up to one chunk of the "wrong" side of the
  boundary unless we re-chunk — re-chunking means touching compressed
  data (still no re-encode: just re-packing frames into new 00cb
  chunks). v1: re-pack audio chunks on the cut boundary for AVI output
  (frames concatenated verbatim), fall back to whole-chunk alignment
  if it complicates the engine; verify audible artifact level in M3.

Guards & UX:
- Pre-flight: container seekable? stream pair resolvable (first video +
  first audio; **warn** — do not block — when the file has more streams,
  e.g. stereo + 5.1 or two languages)?
- "Cut lands on non-keyframe" is the norm — status line always shows
  `cut @ FF / effective @ KK (Δ n frames)`.
- Output filename: `suggestedName` or `original - cut.<ext>`, `<ext>` =
  the input container's extension (owner decision: preserve format);
  never overwrite without confirm; report cut size vs expected
  (kept/duration ratio sanity check).
- Cancel-safe; resume not in v1.

---

## 7. cutlist.at client (`Net`)

- `QNetworkAccessManager`, `QUrl` base = `http://cutlist.at`
  (configurable: user may mirror the site; HTTP-only is a site property,
  note in UI: "Data transfer is not encrypted (site is HTTP-only)").
- `search(conds, page, sortBy, asc) → Page<SearchItem>`; `getInfo()`;
  `downloadCul(id) → QByteArray` (uses `raw=1&nohit=1`).
- Timeouts (10 s), one retry, honest error surfacing (site is flaky-ish).
- JSON mapping in a small hand-rolled layer (QJsonDocument is fine —
  read-only, no serialization of our own model back to their API).
- **Smart apply** (the killer feature): after download, match the CUL's
  `ApplyToFile`/`otrkey` against files in a user-chosen *recordings
  folder*: normalize names (replace `_`/`-`, strip extension &
  `.mpg.HQ`-style suffixes), extract `channel`, `date`, `time`
  heuristically; score = (name overlap, channel match, day match).
  Best ≥ threshold → "Apply to `found.mpg`?", else manual file picker.

---

## 8. UI (Qt Widgets, one window)

```
┌──────────────────────────────────────────────────────────────────────┐
│ File | Cut | Cutlist (cutlist.at) | Export | Help        [settings] │
├──────────────────────────────────────────────┬───────────────────────┤
│                                              │  Cut table            │
│                 Video preview                │  #  In      Out   Len │
│        (QOpenGLWidget)                       │  1  02:21  03:05  24s │
│                                              │  2  10:12  10:55  23s │
│  ◄◄  |<-1  ▶/⏸  ->1  ►►   [scrub bar]       │  3  14:02  14:48  26s │
│  00:00:00:00 / 01:20:00:00   25fps  1080i    │  (double click =      │
│                                              │   preview cut)        │
├──────────────────────────────────────────────┴───────────────────────┤
│ Timeline: [▓ kept ▓][░ ad ░][▓ kept ▓]…  keyframe ticks ▽           │
│  ▼ playhead    cut edges draggable    zoom                            │
├──────────────────────────────────────────────────────────────────────┤
│ Status: source: foo.ts 548 MB • 6 cuts • keep 1:02:11 / 1:20:00      │
│         cut 2 → effective start @ 03:05:01 (Δ -12 f)                │
└──────────────────────────────────────────────────────────────────────┘
```

- **Cut table** rows = *removed* segments by default (what's being cut —
  ads) with a view switch to *kept*. Columns: #, in (TC + frame), out,
  duration, source→dest time, verified ✓, note. Row context actions:
  preview, set playhead, delete merge/extend.
- **Timeline**: single video track; kept = solid, removed = hatched;
  keyframe ticks from GopMap; drag edges = trim cut (frame granularity,
  Shift = 1 f, no modifier = snap to keyframe for the *effective*
  start — actually: edges always move in frames; the keyframe ticks
  show why the Δ indicator appears).
- **cutlist.at panel** (dockable): search box + field selector (name/
  channel/author), results table (name, air date, channel, quality,
  #cuts, rating, author), row → "Download & apply". Shows the CUL's
  `errors.*`/`MissingBeginning/End` as warning icons.
- **Drag & drop — the primary entry flow (owner decision)**;
  File→Open dialogs stay as the fallback:
  - drop a video file → open it as the project
  - drop a `.cul` / `.mproject` onto the window with a video open →
    import its cuts (smart-apply warnings if the name matches a better
    file)
  - drop a video **and** a `.cul` together → open + apply in one gesture
- **Keyboard** (cutana style):
  - `←`/`→` ±1 frame (Shift = ±10, Alt = ±1 GOP)
  - `PgUp`/`PgDn` previous/next cut boundary
  - `Space` play/pause, `F` jump to next cut start
  - `I`/`O` mark in/out at playhead → create cut
  - `C` cycle cut mode at playhead (start-ad / end-ad style quick cut)
  - `Del` remove selected cut, `V` mark verified
- **Status bar** always: TC (H:MM:SS.FF), fps, keyframe Δ for the
  boundary under the cursor/hover.
- Light theme default, dark optional (Qt6 Fusion + palette).
- **Settings dialog (v1)**: language, cutlist.at base URL, output
  container override (advanced; default = input format), disabled
  *Quality mode* entry (v2 re-encode placeholder).
- **Language: German primary, English at parity (owner decision)** —
  both `.ts` translations shipped from day one, `de` the default.

---

## 9. Packaging & portability

- **Toolchain**: CMake ≥ 3.21, C++20, Qt 6.5+ (Widgets, Network,
  OpenGL), FFmpeg ≥ 6 (shared or static).
  - Linux: FFmpeg from distro dev packages for AppImage build
    (nix-like env); ship Qt libs via `linuxdeployqt`.
  - Windows: MSVC 2022 + Qt (MSVC build), FFmpeg from the gyan/dev
    build (shared → DLLs bundled next to exe).
- **Artifacts**:
  - Linux: `MonkeyCut-x.y.z-x86_64.AppImage` (+ tarball for
    no-FUSE systems), later: .deb/.rpm and flatpak (v2).
  - Windows: `MonkeyCut-x.y.z-win64.zip` (self-extracting optional) +
    `.msi`/Inno Setup installer (v2).
- **Config & cache**: `%APPDATA%\monkeycut` / `~/.local/share/monkeycut`;
  per-video GopMap cache in app data keyed by (path, size, mtime,
  duration hash) so no files pollute recordings folders.
- **CI**: GitHub Actions matrix `ubuntu-latest` + `windows-latest`:
  build + unit tests; artifact upload per platform. (No auto-release
  in v1.)
- **No runtime deps** beyond OS-provided: no ffmpeg.exe, no python,
  no node, no VLC.

---

## 10. Project layout

```
monkeycut/
├─ CMakeLists.txt
├─ cmake/            (Qt, FFmpeg find-modules, deploy scripts)
├─ src/
│  ├─ core/          # Qt-free: Time, Fps, Cutlist, CulFile, CutPlanner,
│  │                 #   GopMap, Project (json), unit-testable
│  ├─ av/            # AvProbe, GopScanner, Player, CuttingEngine
│  ├─ net/           # CutlistAtClient
│  ├─ ui/            # MainWindow, VideoView, CutTableView, TimelineView,
│  │                 #   CutlistAtPanel, ExportDialog, About
│  └─ main.cpp
├─ tests/            # QtTest: CulFile round-trip, CutPlanner, TimeCode,
│                    #   CutlistAtClient against record fixtures
├─ resources/        # icons, .ts translations
└─ docs/DESIGN.md    # this file
```

Testing stance: **core is Qt-free on purpose** — CUL parse/serialize
against captured cutlist.at fixtures, CutPlanner boundary math,
timecode rendering get pure unit tests; AV layer gets integration tests
against generated 5 s test streams (ffmpeg CLI in CI, once, to make
fixtures — that's the only place a CLI may be used); engine output
verified by re-probing (stream copy identity: same codec/params,
duration delta, decodable via test decode).

---

## 11. Milestones

| M | Deliverable | ~ effort |
|---|---|---|
| M0 | CMake skeleton, CI (build+test, both OSes), app launches | S |
| M1 | Open video → probe, GopMap, Player: play/seek/±1 frame, audio | L |
| M2 | Cutlist model + CUL import/export + cut table + timeline + I/O cuts + "preview through cuts" + drag & drop entry flow | L |
| M3 | CuttingEngine stream-copy export (TS/PS and **AVI in/AVI out** paths), export dialog, progress/cancel | M |
| M4 | cutlist.at: search UI, download, smart-apply, warnings (errors/missing head/tail) | M |
| M5 | Packaging (AppImage / Win zip), translations (de/en), README, polish | M |

Suggested first vertical slice per milestone: M1 → demo one hand-made
2-cut stream-copy export before building the full UI on top.

---

## 12. Decisions (locked with owner, 2026-08-15)

1. **App name: MonkeyCut.**
2. **Output container = input container.** Never switch formats by
   default; AVI in → AVI out, TS in → TS out, etc. (§6.4). Manual
   override exists only as an advanced option in the export dialog.
3. **UI language: German primary, English at parity**, `de` default,
   both `.ts` files from day one.
4. **Re-encode: out of v1.** Disabled "Quality mode" entry stubbed in
   Settings for the v2 implementation.
5. **cutlist.at read-only in v1** (search + download). The
   `X-Authorization` Bearer hook stays in `CutlistAtClient` so
   login/rating/upload can be added in v2 without refactoring.
6. **Stream selection: first video + first audio**, status-bar warning
   when the file carries more streams (no track picker in v1).
7. **VFR sources: allowed with warning**; file gets a badge, cuts are
   tagged *accuracy-limited*.
8. **Drag & drop is the primary entry flow** (§8): video → open,
   `.cul`/`.mproject` → import onto open video, both together →
   open + apply. File dialogs remain as fallback.

### Follow-ups (during build, not design-blockers)
- Verify `search-by` `field` values beyond `name` (channel/author/
  otrkey) against the live site in M4.
- Empirically check AVI stream-copy cut output (index, `00cb`
  re-packing at joins, artifact level in VLC/WMP) in M3.
- Consider scoring "smart apply" name matching with `item.duration`
  vs local file length (cheap, likely worth it).
- Decide QOpenGLWidget vs plain QImage fallback priority after first
  player prototype (M1) — keep the render target abstract.