# Third-party components

Pinned source URLs and SHA-256 content hashes are recorded in deps.lock.json.
Upstream MIT license texts are in third_party/licenses/.

- doctest 2.4.11: C++ tests.
- nlohmann/json 3.11.3: transport and evidence serialization, not core numerics.
- cpp-httplib 0.18.1: reserved for the local HTTP transport.
- Three.js: browser geometry and camera controls.

The C++ numerical implementation is newly authored with Codex assistance.

## Web foundation
React/React DOM 18.3.1 (MIT), TypeScript 5.7.3 (Apache-2.0),
Vite 6.4.5 (MIT), React type declarations (MIT). Installed dependency
versions, manifest hashes and registry integrity: web/dependency-inventory.json.
Runtime licence texts are collected under third_party/licenses/web/.

Browser-test tooling: @playwright/test 1.56.1 and matching Playwright packages
(Apache-2.0). Test-only; not shipped in the application ZIP.

Viewer: Three.js 0.180.0 (MIT), d3-scale-chromatic 3.1.0 (ISC),
with pinned TypeScript declarations. Details in web/dependency-inventory.json.

Design audit tools: axe-core and Lighthouse, development only. See web/package-lock.json for resolved versions and registry integrity.
