# Solver Lab interface

The browser edition pairs the temperature field with the residual history. Choose a geometry and grid, run all three methods, then inspect the field or export the completed study. The last completed geometry and resolution stay visible while editing the next experiment. Export is disabled when the choices differ from the completed study or a solve is running.

## Visual system

The page uses a 1240 px content column, paired desktop panels and stacked mobile panels. The empty temperature panel reserves room before the first calculation. Ink surfaces and warm paper share the same hierarchy. Ember marks the primary action, glacier marks focus and selection, and Inferno maps temperatures between the selected field's measured minimum and maximum. Solver series use consistent swatches and different line patterns. System font fallbacks keep the interface available offline. Readouts use tabular monospace numerals. Theme initialization runs locally before first paint.

## Interaction and motion

The field contains one instanced cell per recorded temperature. The section plane clips the display at a cell boundary. Node stepping follows visible cells, works with buttons or arrow keys, and reports the original coordinates. Temperature and coordinate rounding applies to the readout only. The WebGL fallback projects measured cells into a static view.

The colour reveal and chart reveal introduce the completed solution over 1.2 seconds. The count-up ends at the measured iteration count. Camera transitions take 600 ms. Idle orbit stops during interaction or inspection. Pause motion and the system reduced-motion preference disable automatic movement. Rendering suspends in a hidden tab. The worker exports final temperatures and residual histories. Time-resolved temperatures are unavailable.

## Review evidence

One paired Chrome 157.0.8103.79 audit of production bundles on the development Mac:

| Measure | Original | Revised |
| --- | ---: | ---: |
| Lighthouse mobile performance, initial page | 99 | 99 |
| Lighthouse mobile accessibility, initial page | 95 | 100 |
| Field frames per second during scripted drag | 29.83 | 33.24 |
| Console or page errors recorded during the journey | 0 | 0 |

The drag used a 360 px desktop-browser viewport, device pixel ratio 1 and the 152-cell constricted block. Frame counts sample display frames containing an instanced field draw. Input pacing and the development machine affect this number. A physical phone requires separate measurement. Both themes passed the horizontal overflow check at 360, 768, 1024, 1440 and 1920 px. Audit output includes desktop and mobile captures and the raw Lighthouse reports in `results/ui-review/`.

For each geometry at grids 3, 6, 9 and 12, all exported values apart from elapsed time matched the archived browser solver exactly. A separate native comparison verified 24 method/configuration pairs and four invalid input cases. Native timing is outside this comparison.

## Repeat the measurements

From `web/`, build the original demo at revision `a9d8f50` and copy the contents of its `dist-demo/` to `../results/ui-review/before/`. Build the revised demo with `npm run build:demo`. Run `node tests/ui-review.mjs before` and `node tests/ui-review.mjs after` serially on the same machine. The commands use local Chrome and a temporary loopback server. The report identifies the browser version and the measurement conditions.
