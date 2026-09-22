# fishtank

An interactive school of fish, simulated and rendered entirely in C++,
compiled to WebAssembly for [imlamont.com](https://imlamont.com). Boids-style
flocking + a small custom renderer talking straight to WebGL2/GLES3 — no JS
rendering framework in the loop.

## Boundary with the website

This module only simulates and renders. It knows nothing about the feed
button's UI, the visitor name/email log, or any network calls — those live
in the website repo's own JS/HTML. This repo exposes a small C API and a
canvas; the embedding page drives it:

```c
int  ft_init(const char* canvasSelector, int widthPx, int heightPx); // 1 = ok
void ft_frame(float dtSeconds);           // call once per requestAnimationFrame
void ft_resize(int widthPx, int heightPx);
void ft_feed_at(float ndcX, float ndcY);  // click/tap position in NDC (x/y in [-1,1], y-up)
void ft_orbit(float dx, float dy);        // drag delta as a fraction of canvas size
void ft_set_fish_count(int count);
```

See `web/index.html` for the reference wiring — that's the whole JS contract
the website needs to reimplement (canvas setup, resize, RAF loop, click ->
`ft_feed_at`, drag -> `ft_orbit`). It also has the click-vs-drag
disambiguation (a threshold on total pointer movement between down and up)
that keeps a small accidental drag from swallowing a feed click, and vice
versa — reimplement that too, don't just wire `ft_feed_at` to `pointerdown`.

## Layout

```
include/fishtank/   Headers: math3d, boids (sim), fish_mesh, environment_mesh
                     (floor/walls/plant blade/water grid), plants (scatter
                     data), shader, renderer, app, gl_compat/gl_loader
src/                 Implementation + the two entry points:
                       main_web.cpp     Emscripten build, exports the C API above
                       main_native.cpp  GLFW desktop window, for fast local iteration
web/index.html       Minimal browser test harness (loads fishtank.js/.wasm, wires the button + canvas)
CMakeLists.txt       One file, two targets depending on whether Emscripten is the active toolchain
```

There's no GLEW/glad dependency for the native build — `src/gl_loader.cpp` is
a ~30-function loader for exactly what this project calls, resolved at
runtime via `glfwGetProcAddress`. Under Emscripten, `<GLES3/gl3.h>` provides
the same calls as real linked symbols, so `gl_compat.h` picks the right path.

## Building

### Native (fast iteration, no browser needed)

```bash
sudo apt-get install -y cmake libglfw3-dev   # once
cmake -S . -B build-native
cmake --build build-native -j
./build-native/fishtank_native   # click to feed, drag to orbit, Esc to quit
```

It can also dump a single frame to a PPM and exit — no display/browser
needed at all, works fine under `xvfb-run` — which is the fastest way to
check a rendering change:

```bash
FISHTANK_SCREENSHOT=/tmp/shot.ppm ./build-native/fishtank_native
```

### WebAssembly

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk   # once
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest    # once
source ~/emsdk/emsdk_env.sh

emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
```

Produces `build-web/fishtank.js` and `build-web/fishtank.wasm`. Those two
files are what get copied into the website repo as a resource — nothing else
from this repo ships.

To try it in a real browser before copying anything over:

```bash
cp build-web/fishtank.js build-web/fishtank.wasm web/
cd web && python3 -m http.server 8934
# open http://localhost:8934/
```

## Simulation notes

- `Boids::update()` is plain O(n²) neighbor search (separation/alignment/
  cohesion) plus a food-seeking force and a soft boundary push-back. Fine at
  the current ~100-150 fish scale; would need spatial partitioning well
  beyond that.
- Food pellets drop from the tank surface at the clicked/fed x/z, sink, and
  either get eaten (fish within `kFoodEatRadius`) or expire after
  `kFoodLifetime` seconds.
- The whole school behaves as one flocking body and can drift toward a tank
  wall over time — that's realistic boids behavior, not a bug. The camera
  distance is derived each frame from the tank's half-extents and the
  current aspect ratio (`Renderer::render` in `src/renderer.cpp`) so the
  full tank stays framed regardless of the embed's width/height. Dragging
  orbits the camera around the tank's vertical axis (yaw, unbounded) and
  adjusts its downward tilt (pitch, clamped to roughly 11-86 degrees —
  see `addOrbitDelta` in `renderer.cpp` — a real vertical look range from
  near water-level to near top-down, not just a narrow safety margin);
  there's no more automatic idle motion now that the camera is interactive.
- Fish are procedural low-poly meshes (`fish_mesh.cpp`), not sourced assets,
  instanced via a single dynamic-per-frame instance buffer (position +
  orientation + per-fish hue color).
- The sandy floor (jittered grid), 4 glass walls, plant blades, and the
  water surface are all procedural too (`environment_mesh.cpp`), built once
  from the tank's half-extents — nothing per-frame to regenerate. Plants
  are scattered in loose random clusters at startup (`plants.cpp`, `Plants`
  class) and sway entirely in the vertex shader (`kPlantVertSrc` in
  `renderer.cpp`) driven by each instance's own phase/amplitude/speed — no
  per-frame CPU work scales with plant count.
- The water surface ripples via the same technique (vertex-shader
  displacement using a small sum of sines, `waterHeight()`), and clicking
  resolves against that *actual animated surface* — `pickSurfacePoint`
  raymarches the click ray against it rather than intersecting a flat
  plane, so feeding stays accurate to what's on screen even as the surface
  moves. The C++ and GLSL copies of the ripple formula must stay in sync
  (see CLAUDE.md).
- Walls and the water surface render last, alpha-blended, with depth writes
  off so they don't occlude the fish/floor/plants behind them, and with
  backface culling off so both are visible from either side (the tank's
  glass from odd orbit angles or from inside; the water surface if the
  camera ever ends up beneath it at the wide pitch range the orbit
  supports). Walls are tinted per-vertex — white above the waterline
  (`Boids::waterSurfaceY()`), blue below it — computed in the wall
  shader from world height relative to that same animated surface.
