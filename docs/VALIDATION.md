# Local validation

These are historical validation results from before the automated test sources and CTest configuration were removed. The test commands described below are no longer available in the current project.

Validated on Windows with MinGW-w64 GCC 16.2, CMake, Ninja, and Python 3.14.

| Check | Result |
|---|---|
| Release core, network, server, desktop, and test targets | Compiled successfully |
| 33 core checks | Passed |
| Real TCP integration suite in Release | Passed |
| Independent Debug headless build and same test suite | Passed |
| clang-format 20.1.8 dry-run with warnings as errors | Passed |
| Desktop joining a server room | Verified through the interface |
| Pen and rectangle mouse drags | Verified through the interface |
| Snapshot rendering of a 32-shape sample diagram | Verified through the interface |

Network tests include a C++ two-client probe as well as Python peers. They verify fragmented reads, accepted-edit replication, room isolation, identity assignment, undo ownership, cursor messages, invalid-edit rejection, oversized-header rejection, reconnect snapshots, server autosave, and loading after restart.

The README screenshot comes from the actual application displaying a diagram sent through the networking protocol. The sample is included under examples.

Not locally verified: MSVC/Linux/macOS runtime behavior, hosted GitHub Actions, sustained load, malicious-peer fuzzing, power-failure durability, or public internet deployment. The implementation intentionally has no TLS or authenticated users and is intended for trusted networks. See SECURITY.md.

## Save/load improvements (October 30, 2026)

Release desktop, server, and test targets were compiled with MSVC 19.51 on Windows.
`ctest --preset release` passed the core and real TCP integration suites
(including the C++ client probe and autosave/restart persistence).

Changed C++ files pass clang-format checks.

VectorGL's build-time clang-tidy check failed with the installed MSVC/tooling
combination; this build used `cmake --preset release -DENABLE_CLANG_TIDY=OFF`.
The new desktop shortcuts were compiled but have not been manually exercised.
These results do not verify POSIX filesystem behavior or power-loss durability.

## Toolbar and light canvas update

The desktop and server compile with MSVC in Release. The light interface and floating
six-tool toolbar were visually inspected; the 4 shortcut was checked in the app
and selected Arrow correctly. The README screenshot was updated from the running app. One-off TCP checks verified preview and
commit replication for rectangle, diamond, ellipse, arrow, line, and freehand,
rejection of invalid point counts, owner undo, and archive persistence. Changed C++
files pass clang-format checks. No automated test folder was added.
