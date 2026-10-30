# Third-party dependencies

Source dependencies are fetched at pinned revisions by CMake; they are not copied into this source repository.

| Project | Role | License |
|---|---|---|
| [VectorGL](https://github.com/mahtab04/vectorgl) | GPU vector graphics | MIT |
| [Asio](https://github.com/chriskohlhoff/asio) | Asynchronous networking | Boost Software License 1.0 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing and serialization | MIT |
| [GLFW](https://github.com/glfw/glfw) | Desktop window and input | zlib/libpng |
| [stb](https://github.com/nothings/stb) | Font/image functionality through VectorGL | MIT or public domain |
| [libtess2](https://github.com/memononen/libtess2) | Path tessellation through VectorGL | SGI Free Software License B |
| [glad](https://github.com/Dav1dde/glad) | OpenGL loader through VectorGL | MIT; generated specification material has Khronos terms |

VectorGL's hinted FreeType text path is disabled in this build. Consult the fetched sources for complete notices. Windows binaries also require the GNU compiler runtime DLLs; their redistribution license notices are included with the separate Windows package.
