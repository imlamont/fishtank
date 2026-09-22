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
                     (floor/walls/plant-blade meshes), plants (plant scatter
                     data, no sim needed — sway is shader-side), shader,
                     renderer, app (platform-independent glue), gl_compat/gl_loader
src/
  main_web.cpp       Emscripten entry point, exports the C API above
  main_native.cpp    GLFW desktop window — click to feed, Esc to quit
  gl_loader.cpp      Native-only: loads GL functions via glfwGetProcAddress
  boids.cpp          Simulation: separation/alignment/cohesion + food-seeking
                      + soft boundary. Plain O(n²) neighbor search.
  environment_mesh.cpp  Floor (jittered grid), 4 walls, plant blade — all
                      built once at startup from the tank's half-extents
  plants.cpp         Random cluster scatter of plant instances (position,
                      height, hue, sway phase/amplitude/speed) — static data,
                      no update() needed
  renderer.cpp        Two shaders (main + plant-with-sway), instanced draw
                      calls, adaptive camera framing, opaque pass then
                      alpha-blended glass walls last
web/index.html       Reference JS harness — the whole contract a page needs
                     to drive this module (canvas setup, resize, RAF loop,
                     click -> ft_feed_at). Website's fish.html re-implements
                     this wiring against the real site's UI/backend.
CMakeLists.txt       One file, two targets, switched on whether Emscripten
                     is the active toolchain (EMSCRIPTEN cmake var)
```

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
- **Floor/wall/plant geometry is generated once in `Renderer::init()`**,
  not rebuilt per frame — they're static (floor/walls) or shader-animated
  (plants), so there's nothing per-frame to regenerate. If a future change
  needs the tank shape to be dynamic, that assumption has to be revisited
  everywhere `init()` currently bakes in `sim.tankHalfExtents()`.

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
  camera through the click point and intersecting the tank's water-surface
  plane (`Renderer::pickSurfacePoint`, `include/fishtank/math3d.h`'s
  `invert()`). That's only numerically stable if the camera looks down at a
  steep-enough angle: with a near-level camera, a ray traveling mostly along
  -Z barely changes height per unit of depth, so solving "where does this
  ray cross the surface plane" required extrapolating way outside the tank
  (confirmed by printing the picked world coordinates — they came out in the
  tens to hundreds of units for a tank with half-extent ~6). The fix was the
  camera's `tilt` in `computeViewProj` (currently ~52°, chosen to clear the
  FOV's half-angle by a wide margin — even the top-of-frame ray needs a
  solidly downward angle, not just the center ray). If the tilt, FOV, or
  aspect-fit math ever changes, re-check picking at the frame's edges and
  corners, not just dead center — center clicks looked fine even when edge
  clicks were wildly wrong.
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
