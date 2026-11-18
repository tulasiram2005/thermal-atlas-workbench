# Solver Lab verification checkpoint — 18 November 2026

## Source changes

- Added explicit, stable accessible names for the Geometry and Grid resolution controls. The browser audit now selects these controls by their labels.
- Kept browser calculations in a Web Worker and included the WebAssembly module, worker and solver assets in the static demo build.
- The Vercel build target is `web/dist-demo`; the demo contains the Solver Lab, not the full desktop workbench.

## Checks completed

| Check | Result | Scope |
|---|---|---|
| TypeScript and Vite static demo build | Passed | `npm --prefix web run build:demo`; GeometryViewer chunk warning remains (509.05 kB minified, 130.04 kB gzip). |
| Native Release CMake build | Passed | All native targets built with AppleClang 21.0.0. |
| CTest | 4/4 passed | Server security, atomic exports, numerics and launcher scaffold smoke. Development Mac. |
| Native/WebAssembly parity | Passed | 24 solver comparisons across 4 grid sizes × 2 geometries × 3 methods; 4 invalid-input checks rejected. Maximum temperature difference: `5.97e-13 °C`. |
| Interactive browser check | Passed manually | Local built demo: default constricted case (152 cells) and cube n=12 (1,728 cells) each reported all 3 solvers converged; JSON evidence download completed. Browser times are not used as benchmark results. |
| Production deployment and assets | Passed | Vercel Hobby production deployment `dpl_G5JQ5vRscT8xYP9o1TbxedXZNfL6` from commit `dae93b2` reported READY. `https://thermal-atlas-workbench.vercel.app/` returned HTTP 200; HTML, JS, CSS, icon, worker and WebAssembly files all returned HTTP 200 with the configured CSP. |

## Unresolved verification

- Playwright's automated Chrome launch exits before the browser audit begins in this environment. The prior automated report is retained as failed; the manual interaction above does not stand in for the full screenshot/axe/browser script.
- The deployment-specific preview URL redirects to Vercel sign-in. The production alias above is publicly reachable; full browser-interaction regression against that hosted alias remains unverified.
- No new Windows run was started. The Windows results in `passscience-workbench` belong to its recorded commit and do not validate the current Thermal Atlas browser/WASM changes.
- Constricted-geometry mesh independence, controlled timing repeats and human screen-reader acceptance remain open.

## Evidence interpretation

The parity result supports numerical consistency between the current native and WebAssembly builds for the tested input matrix. It does not establish accuracy against an independent physical measurement, universal solver superiority, mesh independence, production readiness or complete accessibility conformance.
