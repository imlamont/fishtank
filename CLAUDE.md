# CLAUDE.md — fishtank

An interactive school of fish: C++ boids simulation + a small custom
WebGL2/GLES3 renderer, compiled to WebAssembly for imlamont.com. No JS
rendering framework in the loop — the wasm module owns the canvas and draws
directly.

This repo is standalone from the website repo (`../website`). It produces
`fishtank.js` + `fishtank.wasm`, which get copied into the website's
`resources/` as a built artifact — nothing else from here ships.

## The boundary with the website (do not blur this)

This module **only simulates and renders**. It exports a small C API and
knows nothing else:

```c
int  ft_init(const char* canvasSelector, int widthPx, int heightPx); // 1 = ok
void ft_frame(float dtSeconds);           // call once per requestAnimationFrame
void ft_resize(int widthPx, int heightPx);
void ft_feed_at(float ndcX, float ndcY);  // click/tap position in NDC (x/y in [-1,1], y-up)
void ft_orbit(float dx, float dy);        // drag delta as a fraction of canvas size
void ft_set_fish_count(int count);
```

The feed button UI, the visitor name/email log, any CAPTCHA, and any network
calls to the logging backend belong in the *website* repo's JS/HTML
(`fish.html` there), not here. `ft_feed_at` only triggers the visual food
drop. If a task starts pulling fetch()/form logic into this repo, stop and
reconsider — that was a deliberate choice, not an oversight.

## Layout

```
include/fishtank/   Headers — math3d (vec3/mat4, no external dep), boids (fish/
                     food sim), fish_mesh (fish + food meshes), environment_mesh
                     (floor/walls/plant-blade/water meshes), plants (plant
                     scatter data, no sim needed — sway is shader-side), shader,
                     renderer, app (platform-independent glue), gl_compat/gl_loader
src/
  main_web.cpp       Emscripten entry point, exports the C API above
  main_native.cpp    GLFW desktop window — click to feed, drag to orbit, Esc
                     to quit. Also has a headless screenshot dump (see
                     "Debugging" below) — no browser needed to check a
                     rendering change visually.
  gl_loader.cpp      Native-only: loads GL functions via glfwGetProcAddress
  boids.cpp          Simulation: separation/alignment/cohesion + food-seeking
                      + soft boundary. Plain O(n²) neighbor search.
  environment_mesh.cpp  Floor (jittered grid), 4 walls, plant blade, water
                      surface grid — all built once at startup from the
                      tank's half-extents
  plants.cpp         Random cluster scatter of plant instances (position,
                      height, hue, sway phase/amplitude/speed) — static data,
                      no update() needed
  renderer.cpp        Four shaders (main, plant-with-sway, wall-with-waterline-
                      tint, water-with-ripple), instanced draw calls, adaptive
                      camera framing, opaque pass then alpha-blended walls +
                      water surface last (culling off for both)
web/index.html       Reference JS harness — the whole contract a page needs
                     to drive this module (canvas setup, resize, RAF loop,
                     click -> ft_feed_at). Website's fish.html re-implements
                     this wiring against the real site's UI/backend.
CMakeLists.txt       One file, two targets, switched on whether Emscripten
                     is the active toolchain (EMSCRIPTEN cmake var)
```

## Debugging

The native build can dump a screenshot headlessly (no browser, no display
even — works fine under Xvfb) and exit:

```bash
xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 FISHTANK_SCREENSHOT=/tmp/shot.ppm \
    timeout 3 ./build-native/fishtank_native
python3 -c "from PIL import Image; Image.open('/tmp/shot.ppm').save('/tmp/shot.png')"
```

Prefer this over spinning up a browser for anything that's really about
rendering correctness (geometry, winding, color, shader logic) — it's
faster, and it sidesteps an entire category of browser-environment flakiness
(see the WebGL context-loss gotcha below). Reserve actual browser testing
(Playwright) for things that are genuinely web-specific: the JS glue in
`web/index.html`, `ft_init`'s WebGL context creation, GLSL ES-only
compilation issues.

## Commands

```bash
# Native (fast iteration, no browser needed)
cmake -S . -B build-native && cmake --build build-native -j
./build-native/fishtank_native

# WebAssembly
source ~/emsdk/emsdk_env.sh          # emsdk already installed at ~/emsdk
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
# -> build-web/fishtank.js, build-web/fishtank.wasm

# Try the wasm build in a real browser before copying anything to the website:
cp build-web/fishtank.js build-web/fishtank.wasm web/
cd web && python3 -m http.server 8934   # open http://localhost:8934/
```

`compile_commands.json` at repo root is a gitignored symlink to
`build-native/compile_commands.json`, kept for editor tooling (clangd). Both
`build-native/` and `build-web/` are gitignored build directories.

## Hard rules

- **No GLEW/glad.** `src/gl_loader.cpp` is a ~30-function loader for exactly
  what this project calls, resolved at runtime via `glfwGetProcAddress`.
  GLEW was tried first and dropped — see "Known gotchas" below.
- **Never call `emscripten_set_main_loop`.** JS drives the frame loop via
  `requestAnimationFrame`, calling the exported `ft_frame(dt)` each tick. See
  "Known gotchas."
- **Don't add a rendering framework (Three.js, etc.).** The point of this
  repo is that simulation *and* rendering both happen in C++/wasm.
- **Fish are procedural**, built in `fish_mesh.cpp` from primitives — no
  external mesh/texture assets, no asset-loading pipeline to maintain.
- Masters/large binaries don't belong here at all (unlike the website repo,
  this one has no media pipeline) — if a task wants to add one, it's out of
  scope for this repo.
- **Plant sway happens in the vertex shader, not on the CPU.** The blade
  mesh (`buildPlantBladeMesh`) is authored once, in local space, base at
  y=0 / tip at y=1; `kPlantVertSrc` bends each vertex sideways by an amount
  that grows with that *raw, pre-model-matrix* local y. Don't reintroduce a
  CPU-side per-frame vertex recompute for plants — the whole point of doing
  it this way is that N swaying plants cost the same one instanced draw
  call as N static ones.
- **Floor/wall/plant/water geometry is generated once in `Renderer::init()`**,
  not rebuilt per frame — they're static (floor/walls) or shader-animated
  (plants, water), so there's nothing per-frame to regenerate. If a future
  change needs the tank shape to be dynamic, that assumption has to be
  revisited everywhere `init()` currently bakes in `sim.tankHalfExtents()`.
- **The water ripple formula exists in two places that must stay
  numerically identical**: `waterHeight()` in C++ (`renderer.cpp`, used by
  `pickSurfacePoint`'s raymarch) and its GLSL twin `kWaterHeightGLSL`
  (concatenated into both the wall and water vertex shaders at startup —
  GLSL has no `#include`, hence the string concatenation in `init()` rather
  than a shared source file). If you change one, change the other, or
  clicking will silently stop matching what's actually drawn on screen —
  the kind of bug that's easy to miss because both halves still "work" on
  their own, they'd just quietly disagree about where the water is.
- **`Boids::waterSurfaceY()` is the single source of truth for the still-
  water height** — feedAt's spawn height, the water mesh's rest position,
  and pickSurfacePoint's raymarch target all read it rather than each
  hardcoding `halfExtents.y * 0.92f` separately (that used to be duplicated
  in two places before this existed; don't reintroduce a third copy).

## Known gotchas (found by actually testing, not by reasoning about the code)

- **GLEW's `glewInit()` can fail with "Unknown error" (GLX error 4,
  `GLEW_ERROR_NO_GLX_DISPLAY`) under software/headless GL** (e.g. Xvfb +
  llvmpipe), even though the GL context itself is valid and
  `glGetString(GL_VERSION)` works fine. This is why the native build uses
  its own loader instead of GLEW — it sidesteps GLX display probing
  entirely and is more portable (works the same under EGL/Wayland).
- **Calling `emscripten_set_main_loop()` synchronously from a function
  invoked via `Module.ccall()` throws a JS "unwind" exception** to hand
  control back to the browser event loop. That exception propagates up
  through the `ccall`, silently aborting whatever JS code was written after
  it in the same function — e.g. event listener registration never runs,
  with no visible error unless you check the console. Caught by loading the
  page in an actual browser (Playwright), not by the C++ compiling cleanly.
  Fix: `ft_init` only sets up the GL context; JS owns the RAF loop and calls
  `ft_frame()` itself.
- **A flex child's default `min-height: auto` uses its content's intrinsic
  size**, and a `<canvas>` element's intrinsic size is its `width`/`height`
  *attributes* (the backing store), not its CSS size. A `<canvas>` inside a
  `flex: 1` column can block that column from ever shrinking below the
  canvas's last backing-store size, causing overflow/scrollbars on resize to
  something smaller. Fix: `min-height: 0` on the flex container and the
  canvas's flex-item ancestor. Applies to `web/index.html` and will apply to
  the website's `fish.html` too.
- **Boids cohesion makes the whole school act as one blob** that can drift
  toward a tank wall over time — that's correct emergent behavior, not a
  bug to "fix" by force. The thing that *did* need fixing was camera
  framing: distance is derived each frame from the tank's half-extents and
  the current aspect ratio (`Renderer::render`), not hardcoded, so the tank
  stays fully in frame regardless of the embed's width/height.
- **Feeding at the wrong spot was a camera-angle problem, not a math
  bug.** `ft_feed_at`/`feedAtScreen` resolve a click by ray-casting from the
  camera through the click point and, originally, intersecting the tank's
  water-surface plane analytically (`Renderer::pickSurfacePoint`,
  `include/fishtank/math3d.h`'s `invert()`). That's only numerically stable
  if the camera looks down at a steep-enough angle: with a near-level
  camera, a ray traveling mostly along -Z barely changes height per unit of
  depth, so solving "where does this ray cross the surface plane" required
  extrapolating way outside the tank (confirmed by printing the picked
  world coordinates — they came out in the tens to hundreds of units for a
  tank with half-extent ~6). The fix was the camera's `tilt` in
  `computeViewProj` (currently ~52° at rest, chosen to clear the FOV's
  half-angle by a wide margin — even the top-of-frame ray needs a solidly
  downward angle, not just the center ray). `pickSurfacePoint` has since
  been rewritten to raymarch against the animated water surface instead of
  a flat plane (see the water-mesh gotcha below), but the same underlying
  angle-conditioning issue still applies to it, and the orbit's pitch clamp
  exists for exactly this reason. If the tilt, FOV, or aspect-fit math ever
  changes, re-check picking at the frame's edges and corners, not just dead
  center — center clicks looked fine even when edge clicks were wildly
  wrong.
- **`pickSurfacePoint` raymarches, but only within a small window around an
  analytic estimate — it does not march the whole click ray.** The near/far
  clip points are ~0.1 and ~200 world units out, and marching that entire
  span finely enough to resolve the water ripple's ~0.05-unit amplitude
  would need an impractical step count. So it first solves the *flat*-water
  intersection analytically (same formula as the old plane-only version)
  purely to find *where* to march, then marches a ±2.5-unit window around
  that estimate against the real (rippled) surface height, refining the
  crossing by linear interpolation once bracketed. If the ripple amplitude
  or the tank's proportions ever change substantially, re-check that the
  window is still wide enough to actually contain the crossing.
- Also note: `mx, my`/`clientX, clientY` from GLFW and the DOM are pixels
  from the **top-left**, but NDC is `[-1, 1]` with **+Y up** — the Y axis
  must be flipped when building `ndcY` (`main_native.cpp`'s
  `onMouseButton`, `web/index.html`'s `pointerdown` handler) or clicks land
  vertically mirrored.
- **Wall/floor triangle winding is hand-derived per face, not automatic.**
  With backface culling on, each axis-aligned quad in `environment_mesh.cpp`
  needs a *specific* winding order so its outward normal (away from the
  tank's center) is what actually gets kept — the "default" winding
  (`pushTri(a, b, c)`, `pushTri(a, c, d)`) gives the right normal for the
  left and front walls, but the **opposite** (flipped: `pushTri(a, c, b)`,
  `pushTri(a, d, c)`) is needed for the right wall, back wall, and every
  floor grid cell. This isn't a matter of taste — get it backwards and that
  face is invisible (its front face points into the tank, gets backface
  culled from every camera position outside it). If you add a new static
  world-space panel, work out its winding the same way: cross(b-a, c-a)
  must point in the direction you want visible from outside, not just "some
  consistent direction."
- **Drag-to-orbit reuses the same click-vs-drag disambiguation in both
  `main_native.cpp` and `web/index.html`**: accumulate total pointer
  movement between press and release, and only treat it as a feed click if
  that total stays under a small pixel threshold (currently 4px) —
  otherwise every feed click's inevitable 1-2px of jitter would either
  spuriously nudge the camera or, worse, a real drag's release point would
  also fire a feed. Wire a new input path (touch gestures, a second mouse
  button, etc.) through this same pattern, not a raw `pointerdown` ->
  `ft_feed_at` call.
- **`Renderer::addOrbitDelta`'s pitch clamp (`minTilt`/`maxTilt`,
  currently ~11 to ~86 degrees) trades click-picking precision for a real
  vertical look range, on purpose.** The original fixed 52-degree tilt was
  chosen to clear the FOV's half-angle with a wide margin specifically so
  `pickSurfacePoint`'s ray-plane intersection stayed well-conditioned (see
  the picking gotcha above). Once the camera became user-controllable, a
  clamp that tight made vertical dragging feel like it barely did
  anything, so the range was widened deliberately — accepting that
  clicking near the shallow end (near water-level) can land food less
  precisely than at the original tilt, since the ray-plane math gets
  numerically shakier there. This is safe specifically *because*
  `Boids::feedAt` clamps its result to the tank bounds regardless: the
  failure mode at extreme angles is "food lands at the nearest edge
  instead of exactly under the cursor," not a crash or an out-of-tank
  result. If picking precision at shallow angles ever actually matters
  (rather than just "a bit imprecise is fine"), the real fix is a better
  picking method for that regime (e.g. nearest point on the click ray to
  the tank's AABB), not re-narrowing this clamp back down.
- **A long-lived browser tab (many reloads/navigations in one session) can
  hit `CONTEXT_LOST_WEBGL` from the browser/GPU side, with nothing wrong in
  the code.** Symptom: the page loads, JS reports success (`ft_init`
  returns 1, the status text says "running"), the clear color even shows,
  but nothing ever renders — and the console shows a `WebGL:
  CONTEXT_LOST_WEBGL` warning, easy to miss since it's a warning, not an
  error. Confirmed environmental, not a code bug, by reproducing it with
  the exact last-known-good commit (no water/wall changes at all) in the
  same aged browser session, and *not* reproducing it in a freshly launched
  browser process running the same build. If a browser test ever goes
  inexplicably blank mid-session: check the console for this specific
  warning before assuming the just-written code is at fault, and prefer
  killing/restarting the browser process over debugging the "bug" further.
  This is also a good reason to lean on the native screenshot tool (see
  "Debugging" above) for anything that doesn't specifically need a real
  browser.
