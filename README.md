# ActiveHour

A Pebble watchface that shows **how active you've been, minute by minute, for
the past hour** — at a glance. A ring of 60 dots surrounds the time, one dot
per minute of the hour. Each minute's activity stacks 1–5 dots outward: a
minute you sat still is a single dot; a minute of solid walking is a full
5-dot spoke. Elapsed minutes draw in the theme's active color, upcoming
minutes in the dim color — so the ring is both a clock and an activity trace.

Unlike a step counter, this shows the *shape* of your hour: how long you've
been sitting still, when you last walked, and for how long.

**[Install from the Rebble appstore](https://apps.rePebble.com/932411b4b5ba45ce839ff833)**

Supported: Pebble Time / Steel (basalt), Pebble Time Round (chalk), Pebble 2
(diorite), Pebble Time 2 (emery), Pebble 2 Duo (flint), Pebble Round 2
(gabbro).

---

## How the activity data works

### Steps → dots

Each minute is scored from its step count
(`calculateDotsFromMinuteSteps` in [main.c](src/c/main.c)):

- **0 steps → 1 dot** (the baseline: the minute happened)
- **1+ steps → 2 dots**, plus one more per 30 steps, **capped at 5**

So the outermost dot appears around 90+ steps in a minute — roughly a full
minute of continuous walking. Dots stack outward from the ring radius at 6px
spacing.

### Backfilling the past hour

At launch, `fetchPastMinuteSteps()` calls the Pebble Health API's
`health_service_get_minute_history()` for the last 60 minutes and fills
`s_dotArray[60]`, indexed by wall-clock minute. Two API caveats shape the
code (see the
[HealthService docs](https://developer.repebble.com/docs/c/Foundation/Event_Service/HealthService/#health_service_get_minute_history)):

- The call may return **fewer records than requested**, and individual
  records can be flagged `is_invalid` (watch off wrist, not worn) — an
  invalid record's `.steps` field is undefined garbage and must not be read.
  Both cases get the baseline single dot.
- **The most recent ~15 minutes may not be returned yet.** Minute records
  become queryable in delayed batches — the official health guide's own
  example queries "the last hour, *except the last 15 minutes*". So a fetch
  at launch can leave a gap just behind the current minute.

That gap is why the face refetches once at the next minute where
`tm_min % 15 == 1` — just past a quarter-hour boundary, when the previously
unavailable batch has landed. From launch onward the live delta path (below)
covers new minutes, so history only ever needs to fill in the past.

### Tracking the current minute live

The live path never touches the (expensive) minute-history API. Instead:

1. On every minute tick, the face snapshots `health_service_sum_today()`
   (total steps today), resets the new minute to 1 dot, and zeroes the
   per-minute accumulator.
2. During redraws, `getNumDots()` diffs the current total against the last
   snapshot and accumulates the delta into `s_lastMinSteps` — steps taken
   *this* minute — and converts to dots.
3. `HealthEventMovementUpdate` events mark the canvas dirty between ticks, so
   the current minute's spoke grows in near-real-time as you walk.

(Known wart: `getNumDots()` is called from inside the render pass and mutates
state / updates the step label there. It has worked for years, but it's the
first thing to clean up if redraw behavior ever gets weird.)

### The center readout

- **Time** — the current time (see Fonts below). In 12-hour mode the hour is
  formatted with `%l`, which leaves a **leading space for single-digit hours.
  This is deliberate**: it keeps the colon visually centered so the time
  doesn't shift sideways between 9:59 and 10:00. Do not "fix" it.
- **Steps/sleep line** — if you've taken more than 500 steps today, today's
  step total; otherwise (i.e. overnight/early morning) last night's sleep as
  `7h 42m`, from `HealthMetricSleepSeconds`. Hours/minutes are bounded with
  unsigned modulo so the compiler can prove the string fits its buffer.
- **Date** — `Wed, Jul 22` style.

Steps and date lines are each optional (settings).

## Rendering the ring

`draw_proc` walks all 60 minutes; each dot position comes from Pebble's
integer trig:

```c
x = sin_lookup(TRIG_MAX_ANGLE * m / 60) * radius / TRIG_MAX_RATIO + center.x
y = -cos_lookup(TRIG_MAX_ANGLE * m / 60) * radius / TRIG_MAX_RATIO + center.y
```

The base ring radius (`DOT_DISTANCE`) is per-platform — 60 on the 144×168
watches and chalk, 82 on emery, 87 on gabbro — chosen so the ring sits at the
same relative position on every screen.

Layered on top of the plain dots:

- **Bold dots** — doubles dot radius (1px → 2px).
- **Hour marks** (requires bold dots) — the innermost dot at each clock-hour
  position (`m % 5 == 0`) draws one size smaller, giving 12 subtle ticks.
- **Battery indication** (requires bold dots) — dot *i* (counting outward)
  draws thin when the charge is below `(i+1) × 10%`. Under 50% the 5th dot
  thins, under 40% the 4th follows, down to a fully thin ring under 10%. The
  battery level rides the dots you already have — no extra UI.
- **Weather dot** (optional) — the temperature in °F, shown *positionally*:
  a dot just inside the ring at minute `temp % 60`. 72° = a dot at the
  12-minute mark. Below 0° it's ice-blue, below 60° aqua, above orange. Its
  colors are fixed (not themeable).

### Fit dots & the zoom view (Pebble Time 2 only)

A full 5-dot minute reaches `DOT_DISTANCE + 4×spacing + dot radius` from
center. On emery that overshoots the screen edge, clipping the 3- and
9-o'clock spokes. The **Fit dots** setting pulls the ring in so everything
fits.

Turning it **off** is a real choice, not a regression — the "zoom view": the
ring stays wide (edge dots crop), and the freed center buys a much larger
clock (58px vs 48px) and larger steps/date text (Gothic 24 vs 18). The option
is emery-only because the small screens have no room to trade (a fitted ring
there would run through the digits) and round screens don't clip at all.

## Fonts

Four clock fonts, selectable in settings:

| Font | Source | 144×168 + chalk | emery fitted | emery zoom | gabbro |
|---|---|---|---|---|---|
| Bitham (classic) | system font | 42 | 42 | 42 | 42 |
| Montserrat *(default)* | bundled, OFL-1.1 | 36 | 42 | 58 | 54 |
| Roboto | bundled, Apache-2.0 | 40 | 48 | 58 | 60 |
| LECO | system font | 42 | 42 | **60** | **60** |

Why the sizes differ: the time crosses the vertical center of the face, which
is exactly where the ring's innermost dots pass, so the **ring sets the width
budget**: `2 × (DOT_DISTANCE − dot radius − margin)`. Every size in the table
was measured (`graphics_text_layout_get_content_size` on `"88:88"`, bold
weight) against that budget rather than estimated.

- **Bitham** is firmware-only — no TTF ships in the SDK and it's a commercial
  face, so it can't be bundled and is capped at the firmware's 42px.
- **Montserrat** is the default because it's the closest redistributable face
  to Bitham's geometric look; its wide round digits cost it a size step
  versus Roboto everywhere.
- **LECO 60 is oversized on purpose** — it overlaps the innermost ring dots
  in the zoom view. In emery's fitted view it drops to LECO 42 (60 would be
  buried in the dots). LECO always renders in its regular weight — the Bold
  text setting doesn't change it.

Bundled fonts are subset to `[0-9: ]` (a couple of KB each) and scoped per
platform via `targetPlatforms`, so no watch downloads a size it can't use.
Licenses ship in [resources/fonts/](resources/fonts/) — see
[NOTICE.md](resources/fonts/NOTICE.md).

Per-font pixel nudges for text positioning live in `getLayoutTune()` — a
small table of `{time, step, date}` y-offsets per platform, for dialing in
each face's different internal leading without touching the base layout.

## Settings

Settings are a hosted HTML page ([other/activehour.html](other/activehour.html),
served from GitHub Pages) opened by PebbleKit JS. On save, the page returns a
JSON blob that [index.js](src/pkjs/index.js) forwards to the watch as an
AppMessage.

Details that matter:

- **Message key numbers == persist key numbers** (a historical constraint
  worth preserving). Booleans arrive as `"true"`/`"false"` strings and are
  `strcmp`'d; the six custom-theme colors (keys 18–23) arrive as packed
  `0xRRGGBB` ints and are quantized on-watch to Pebble's 64-color palette
  with `GColorFromRGB` (and to black/white on the B&W watches).
- **The JS drops any key the page didn't send.** A missing field would
  stringify to `"undefined"` and read as *false* on the watch — so saving
  from a stale cached config page would silently switch off every setting
  added since. Filtering missing keys means old pages can't clobber new
  settings.
- **Keys 12–14 are retired** (old daily-color/inverted/bluetooth features)
  and intentionally left unused so ancient installs' persisted values can't
  be misread.
- **Defaults** are written once, behind a `PERSIST_DEFAULTS_SET` sentinel, so
  they only apply to fresh installs: Orange theme; date, steps, bold text,
  bold dots, hour marks, fit dots, battery, Montserrat all ON. **Weather
  defaults OFF** deliberately — it's the one option that triggers a location
  permission prompt, and that shouldn't happen before the user asks for it.

Themes: seven built-in presets (B&W, Orange, Green, Blue, Purple, Red, Teal,
dark background only) plus a full custom theme — six independently pickable
colors (background, time, active dots, dim dots, steps, date).

## Weather pipeline

- The watch never asks for weather unless the setting is on, and `getWeather()`
  in JS re-checks before touching geolocation — **location is double-gated**.
- Fetches from **Open-Meteo** over HTTPS (free, no API key), already in °F.
- On the `ready` handshake the JS announces itself (`KEY_JSREADY`), the watch
  replies with the weather preference, and thereafter the watch requests a
  refresh every 30 minutes.

## Development

Requires the [Rebble tool + SDK](https://help.rebble.io/sdk/) (`pebble` CLI).

```bash
pebble build
pebble install --emulator emery
pebble screenshot --emulator emery shot.png
```

Useful to know:

- The emulator has **no health history**, so the ring renders nearly empty.
  To evaluate ring layout, temporarily seed `s_dotArray` full (set entries to
  5) — remember the real watch fills it from actual data.
- If settings changes don't seem to apply on the emulator, wipe first:
  persisted settings survive reinstalls (`pebble kill && pebble wipe`).
- `SCREENSHOT_RUN` (top of main.c) is a store-screenshot mode: it drives the
  ring from seconds instead of minutes with randomized activity so a full
  ring can be captured in one minute. Never ship `true`.
- Publishing goes through `pebble publish` to the Rebble appstore. The
  release version comes from `package.json`'s `version`, not a CLI flag.

### Repo layout

```
src/c/main.c            the whole watchface
src/pkjs/index.js       PebbleKit JS: config page glue + weather
other/activehour.html   hosted settings page (GitHub Pages serves this path)
resources/fonts/        bundled Roboto + Montserrat subsets, licenses, NOTICE.md
store/                  appstore screenshots and icons
```
