# Handover: Pebble Watchface in the style of the most popular mp3 player in year 2004

**For:** Claude Code
**From:** Design/spec phase (Clover), handing off to implementation
**Status:** Spec finalized (v1.5), visual direction approved via HTML mockup. Nothing has been built on-device yet — this is a from-scratch implementation.

---

## 0. Read this first

This project has two artifacts behind it that you should treat as source of truth:

1. **The spec sheet** (reproduced in full in Section 2 below) — defines every piece of content, data source, layout rule, and behavior.
2. **The approved HTML mockup** (`watchface_mockup.html`, attached alongside this doc) — a browser-based visual simulation of the final design, built with live JavaScript so it ticks in real time off the system clock. It is **not** the implementation — it's a throwaway prototype used to validate layout and behavior decisions before writing any C. Use it as the visual and behavioral reference, but do not try to port its JS/CSS directly; the real implementation is Pebble C SDK with pre-rotated bitmap assets, which is architecturally very different from a CSS/DOM mockup.

If anything in the mockup and the spec sheet conflict, the **spec sheet wins** — the mockup was a design aid and may have simplified or approximated things (e.g., it renders digits as live DOM text, not as bitmap assets; it does not implement the marquee behavior described in earlier design discussion, since that requirement was later dropped in favor of two-line stacked text with no animation — see Section 2, §4.1).

---

## 1. Your first task: environment setup

Before writing any application code, set up a working Pebble C SDK development environment and confirm you can build and run the stock "hello world" watchface in the emulator. Concretely:

1. Install the Pebble SDK (local toolchain, not CloudPebble — we want this buildable headlessly/in CI eventually).
2. Confirm the `pebble` CLI tool works (`pebble --version` or equivalent).
3. Create a new watchface project via the SDK scaffolding.
4. Build it and run it in the emulator targeting at minimum the **Aplite** and **Basalt** platforms (144×168 displays — this is our target resolution per the spec; see §1.1). Do not target Emery/Gabbro (Pebble Time 2 — 200×228) for this version; that is explicitly out of scope (see Section 2, closing note on Time 2).
5. Confirm you can see the default watchface rendering in the emulator before touching any of our code.
6. Commit this working skeleton to the repo as the first commit, before any custom code — so there's a clean baseline to diff against.

**Do not skip or shortcut this step.** If the SDK install or emulator has friction (missing system deps, Python version issues, etc.), resolve it and document what was needed in the repo's README, since this will need to be reproducible.

---

## 2. The spec sheet (source of truth)

> This is the final version (v1.5) of the spec as agreed. Implement against this directly.

### 2.1 Overall Design

**Orientation**
- Content is rotated 90° clockwise, displayed in landscape.
- Design canvas is **168 (width) × 144 (height)**, matching the physical 144×168 portrait display rotated once.
- **Critical architectural point:** rotation happens once, at **asset build time** (pre-rendering bitmaps rotated), not at runtime. There is no runtime rotation transform, no framebuffer manipulation, no `RotBitmapLayer` runtime rotation calls for arbitrary angles. All drawing at runtime is plain 2D positioning of pre-rotated bitmaps onto a 168×144 canvas. Do not introduce any runtime rotation math — if you find yourself needing it, something has gone wrong architecturally and it should be flagged back rather than worked around.

**Visual style**
- Overall look evokes the classic UI of the most popular mp3 player in year 2004 (status bar + info area + progress bar, three-section layout).
- All text and icons are pre-rotated bitmap assets.
- Monochrome (black/white) — target single-color Pebble hardware.

**Scaling principle (important — do not pixel-match the most popular mp3 player in year 2004)**
- Do **not** scale the original 138×110 pixel layout of the most popular mp3 player in year 2004 proportionally and copy coordinates.
- Instead, replicate the *design language and proportions* of the most popular mp3 player in year 2004, not its literal pixel grid:
  - Font size should be set relative to what fits well on the larger 168×144 canvas (~60% more pixels available than the original), not shrunk to match original pixel counts.
  - Spacing between elements within each row is **elastic** — computed from available width at build/layout time, not hardcoded from measurements taken off a screenshot of the most popular mp3 player in year 2004.
  - Extra horizontal space (compared to the original device) is absorbed by stretching/compressing inter-element spacing, not by adding new visual elements to "fill" it.
- Element positioning within each row uses an **anchor + elastic spacing** model:
  - Left-aligned elements: fixed offset from the left edge.
  - Right-aligned elements: fixed offset from the right edge (measured inward).
  - Centered elements: centered based on their own actual rendered width.
  - Evenly-distributed elements (e.g. the 3 status bar items): divide available width into equal thirds, center each element within its third.

### 2.2 Fonts & Assets

**Three asset categories — all closed/finite sets, no general-purpose font needed:**

**A. Digits & symbols** (status bar time, date, progress bar values)
- Digits 0–9
- Colon `:`
- Slash `/` (for date, M/D format)
- Hyphen/minus `-` (used both for compound number words like "twenty-three" and as the negative-sign prefix in the §5.3 Option 2 remaining-time display)

**B. Weekday abbreviations** (fixed set of 7, stored as whole units — not built from a general letter set)
- Mo, Tu, We, Th, Fr, Sa, Su

**C. Spoken-time words** (closed vocabulary, for the two-line hour/minute display — see §4.1)
- Hour/minute-ones words (reused across both): One, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Eleven, Twelve (12 words)
- Minute teens-only words: Thirteen, Fourteen, Fifteen, Sixteen, Seventeen, Eighteen, Nineteen (7 words)
- Minute tens words: Twenty, Thirty, Forty, Fifty (4 words)
- Placeholder word: "Oh" (for single-digit minutes, e.g. "Three Oh Five")
- Top-of-hour word: "O'Clock"
- AM/PM: "AM", "PM" (2 words)
- **Total: ~27 word assets.** All rendered in **Title Case** (confirmed design decision — see §4.1).

**Longest-phrase reference (verified by exhaustive script over all 720 hour×minute combinations):** the longest spoken-time phrase is 19 characters (e.g. "Eleven Twenty-Three"). This originally informed a marquee-scroll design for overflow handling — **that marquee design was later superseded**. The current approved design instead splits hour-word and minute-word onto two separate stacked lines (see §4.1), which keeps every line short enough that overflow essentially does not occur in practice (hour words max out at 6 characters — "Eleven"/"Twelve" — and no minute word exceeds "Twenty-Three" which fits on its own line). **Do not implement marquee/scrolling text.** This was explicitly removed from scope.

**Font choice**
- Visual reference: the original system font of the most popular mp3 player in year 2004, Espy Sans (proprietary, do not use — licensing risk).
- Use **Carthage Sans** instead — an open-licensed (CC BY-SA 4.0 / OFL, dual-licensed) font modeled on Espy Sans' 12pt bitmap design. Visually close to the look of the most popular mp3 player in year 2004.
- **Attribution requirement:** if/when this watchface is published, credit Carthage Sans in the app description per its license terms. This is independent of whether the watchface's own source code is open or closed (Pebble App Store does not require open-sourcing watchfaces — confirmed).

**Asset production pipeline**
1. Render each required string (single chars for category A, two-letter pairs for category B, whole words for category C) using Carthage Sans via Python + PIL.
2. Rotate each rendered glyph/word 90° immediately after rendering (`rotate(90, expand=True)` or equivalent).
3. Crop/center according to the actual target size on the 168×144 canvas (do not reuse the original pixel dimensions of the most popular mp3 player in year 2004).
4. Pack each category into its own sprite sheet (not hundreds of individual resource files) — stay within the Pebble resource limits: **256 resource files max**, **128 KB total on Aplite / 256 KB total on Basalt & Chalk**.
5. This is a **build-time** process. Runtime code only does lookups into the sprite sheet + blit — no rendering, no rotation, at runtime.

### 2.3 Row 1: Status Bar

| Position | Content | Data source | Layout |
|---|---|---|---|
| Left | Mute/Quiet Time icon (two states: active/inactive) | `quiet_time_is_active()` | Left-aligned, fixed margin |
| Center | Current time (digits, e.g. "2:07") | `TickTimerService` / `struct tm`, category A bitmaps | Centered within its third of the row |
| Right | Battery level | `BatteryStateService` | Right-aligned, fixed margin |

Spacing between the three elements is elastic, computed from available width (168px minus margins minus each element's own rendered width).

**Note:** `quiet_time_is_active()` is a **classic C SDK API** (added in SDK 4.3). If at any point this project is reconsidered for the newer Alloy/JS framework instead of classic C, be aware this API has **not** been confirmed to exist in Alloy as of this handover — checked both the Sensors/Input and Device Info pages in Alloy's docs, neither mentions Quiet Time. This project should stay on the classic C SDK specifically because this feature depends on it.

### 2.4 Row 2: Date Info

| Position | Content | Format | Layout |
|---|---|---|---|
| Left | Date | `M/D` (e.g. "6/18") | Left-aligned, fixed margin |
| Right | Weekday | Two-letter abbreviation (category B asset) | Right-aligned, fixed margin |

### 2.5 Row 3: Spoken Time (formerly "song title / artist" area)

**Layout: two stacked lines, not one combined line.**

- **Line 1:** the hour word (e.g. "Three")
- **Line 2:** the minute word (e.g. "Oh Five", "Thirty", "Twenty-Nine")
- Below both: AM or PM indicator (category C bitmap)

This two-line layout was a deliberate late-stage change from an earlier single-line design, specifically to use vertical space that a single line left empty, and to allow larger type per line. It also has the side effect of narrowing where overflow can ever happen — see the "longest-phrase" note in §2.2.

**Time-word generation rules (confirmed: digits-only style, no "past/to/quarter/half")**

| Case | Rule | Example |
|---|---|---|
| Top of the hour (0 min) | `[Hour] O'Clock` | "Three O'Clock" |
| 1–9 minutes | `[Hour]` / `Oh [minute-ones-word]` | "Three" / "Oh Five" |
| 10–19 minutes | `[Hour]` / `[teens-word]` | "Three" / "Thirteen" |
| 20+ minutes, exact ten | `[Hour]` / `[tens-word]` | "Three" / "Thirty" |
| 20+ minutes, not exact ten | `[Hour]` / `[tens-word]-[ones-word]` | "Three" / "Twenty-Nine" |

All words rendered in **Title Case**. 12-hour format. AM/PM determined from `struct tm.tm_hour` (`tm_hour < 12` → AM), independent of the system's 12h/24h display preference (`clock_is_24h_style()` is **not** used for this — this watchface always speaks in 12-hour terms for the spoken-time line, regardless of what the digit clock in the status bar shows).

**No animation / no scrolling.** This was explicitly requested and confirmed. Earlier design discussion explored marquee-style scrolling (to replicate the behavior of the most popular mp3 player in year 2004 for overflowing song titles) with three user-selectable modes (no scroll / scroll-once / periodic scroll) — **this entire feature was dropped** in favor of the two-line static layout. Do not implement any scrolling/animation logic for this row. If you encounter any reference to marquee behavior elsewhere, it is stale — the two-line static design supersedes it.

### 2.6 Row 4: Progress Bar

Mid-row: a horizontal progress bar. Below it, left and right labels showing numeric values related to the current time segment. **User-selectable in two independent dimensions** (both configurable, Clay config page):

**Dimension 1 — what the progress bar measures:**
- **Mode 1:** Progress within the current **minute** (0–59 seconds elapsed)
- **Mode 2:** Progress within the current **hour** (0–59 minutes elapsed)

**Dimension 2 — what the bottom labels show:**
- **Option A:** Segment start / segment end (e.g. for hour mode: left = "3:00", right = "4:00")
- **Option B:** Elapsed / remaining, remaining prefixed with a minus sign, **same `MM:SS` shape for both modes** (confirmed — the minute-mode version is NOT bare seconds, it uses the same two-part zero-padded format with a leading `00:` for the minutes component):
  - Hour mode example (current time 03:29:18): left = `29:18`, right = `-30:42`
  - Minute mode example (any time at second 18 of the current minute): left = `00:18`, right = `-00:42`

Both dimensions are independently configurable — 2×2 = 4 total display combinations, all of which must work.

### 2.7 Future Features (explicitly OUT of scope for this build — do not implement)

- **Custom time-segment progress bar** (user-defined daypart boundaries, e.g. 7:00/9:00/12:00/18:00/22:00, progress shown within whichever segment contains the current time). Noted here only so you don't accidentally half-build toward it — leave hooks minimal or absent.
- **Step-count health progress bar** (today's steps vs. user-set daily goal, via `HealthService`). Same — out of scope, do not implement, but be aware it's a likely future addition if you're naming things like enums for progress-bar modes (leaving room to extend the mode enum later is reasonable; actually wiring up HealthService is not this build's job).

### 2.8 Config (Clay, phone-side settings page)

For this build, the Clay config page needs:
- Progress bar mode selector (Minute / Hour — §2.6 Dimension 1)
- Progress bar label format selector (Segment start/end / Elapsed-remaining — §2.6 Dimension 2)

No other settings are in scope for this version.

### 2.9 Non-functional requirements

- Refresh on the relevant tick granularity: second-level when minute-mode progress bar is active (since it needs per-second updates), minute-level otherwise. Follow standard Pebble power-efficiency practice — don't force second-tick subscriptions when not needed (i.e. if the user has selected Hour mode, you likely only need minute-granularity ticks, not second-granularity, except for whatever row needs live seconds in Option B's label — check whether Hour mode + Option B requires seconds display: **yes it does**, per the `29:18` example above, so second-tick subscription is needed whenever Option B is selected, regardless of bar mode).
- Total resource budget: stay within 128 KB (Aplite) / 256 KB (Basalt/Chalk) including all sprite sheets.
- Quiet Time detection has a hardware/OS dependency — handle gracefully if unavailable (shouldn't be an issue on any real hardware/firmware running SDK 4.3+, but don't crash if the call fails).

### 2.10 Publishing / licensing notes (informational, not a build task)

- Pebble App Store does not require watchfaces to be open source (source code URL is an optional field, not a required resource). This watchface can be published closed-source.
- Carthage Sans font requires attribution per its license — add a credit line to the app store description when publishing (not a code task, just don't forget it's needed later).

### 2.11 Explicitly out of scope: Pebble Time 2 / Emery

Pebble Time 2 (Emery platform) has a larger 200×228 display. Pebble OS has some form of automatic scaling/letterboxing for old apps on the new hardware (reporting varies between "auto-scales to fill" and "shows bordered/letterboxed until the app is updated" — exact behavior unconfirmed, may be version-dependent). **This build should target Aplite/Basalt (144×168) only.** Do not attempt Emery-native support, do not test against Emery, do not add resolution-detection branching for it. If this needs to be revisited later, the elastic-layout architecture (§2.1 scaling principle) should make a future port easier, but that's not this task.

---

## 3. Implementation order (suggested)

1. Environment setup + hello-world commit (Section 1 — do this first, no exceptions).
2. Asset pipeline: write the Python/PIL script that generates the three sprite sheets (categories A, B, C per §2.2). Commit the script *and* its generated output into the repo (see Section 4 — build artifacts get committed).
3. Status bar (Row 1) — simplest row, good first integration test of "bitmap lookup + blit + elastic layout" pattern that every other row will reuse.
4. Date row (Row 2).
5. Spoken-time row (Row 3) — includes the time-word generation logic; this is pure logic (no new rendering pattern beyond what Row 1/2 established) but has the most edge cases (verify against the full 0–59 minute × 1–12 hour matrix, 720 combinations — a quick unit test enumerating all of them against expected strings is worth writing).
6. Progress bar (Row 4) — both dimensions, all 4 combinations.
7. Clay config page wiring.
8. Full integration test in emulator across both target platforms (Aplite, Basalt).

## 4. Repo / commit expectations

- Commit the working SDK skeleton first, before any custom code (Section 1).
- Commit the asset-generation script.
- **Also commit the generated build output** (the compiled `.pbw` and/or the generated sprite sheet image assets) — do not rely solely on "regenerate on build." Check the generated sprite sheets into the repo alongside the script that produces them, so the repo state is reproducible and inspectable without re-running the pipeline. If generated assets are large, note that in the README, but still commit them for this project (no `.gitignore` exclusion for build output here — that's a deliberate instruction, not an oversight).
- Standard commit hygiene otherwise — reasonably scoped commits per logical step above, not one giant commit.

## 5. Open questions to flag back (do not guess silently)

- Exact pixel dimensions for each row's height (status bar / date row / text row / progress row) within the 144px total — the spec defines proportional/elastic rules but not final numbers. Pick reasonable values consistent with §2.1's scaling principle, but flag the chosen values back for review rather than treating them as final.
- Whether "O'Clock" needs special-case narrower layout handling (it's the longest single minute-line token: 7 characters) — check it fits comfortably at whatever font size gets chosen for Row 3 before finalizing that size.
