# Vulkan infinite procedurally generated terrain

## About

This is a **personal playground** for an infinite procedurally generated terrain renderer in Vulkan. No guarantee that the code will work.

## Screenshots

<a href="screenshots/01.jpg"><img src="screenshots/01.jpg" height="128x"></a>
<a href="screenshots/02.jpg"><img src="screenshots/02.jpg" height="128x"></a>
<a href="screenshots/03.jpg"><img src="screenshots/03.jpg" height="128x"></a>
<a href="screenshots/04.jpg"><img src="screenshots/04.jpg" height="128x"></a>
<a href="screenshots/05.jpg"><img src="screenshots/05.jpg" height="128x"></a>
<a href="screenshots/06.jpg"><img src="screenshots/06.jpg" height="128x"></a>
<a href="screenshots/07.jpg"><img src="screenshots/07.jpg" height="128x"></a>
<a href="screenshots/08.jpg"><img src="screenshots/08.jpg" height="128x"></a>

## Videos

https://www.youtube.com/watch?v=56WGJljkwuk (Older version)

## Some technical background

* Uses multi-threading for generating newly visible terrain chunkgs
* Draw batches for visible objects (trees, grass) are generated per-frame on the GPU
* "Old school" render-to-texture reflections
* All terrain settings can be changed on the fly
* Renderer
    * Requires Vulkan 1.3
    * Dynamic rendering
    * Single-pass shadow cascade generation using multi view
    * Uses MSAA and alpha-to-coverage for order-independent transparency

