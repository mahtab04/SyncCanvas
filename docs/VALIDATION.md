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

## Controls and security update (October 31, 2026)

- Release server, network, core, and desktop targets compiled with MSVC.
- One-off TCP checks passed for missing/wrong keys, authorized preview/commit,
  room isolation, generic malformed-field errors, hidden archive parser details,
  rejection of whitespace-padded frame floods, and a fixed 10-second join deadline
  despite pre-join traffic. Joined peers remained connected past that deadline.
- Archive output was checked for absence of the synthetic access key. Invalid
  server key configuration failed closed without echoing its value.
- A separate C++ probe exercised the actual NetworkClient access-key join,
  preview, commit, disconnect, and rejected-key paths.
- 12,000 constrained-geometry cases checked board boundaries, equal shape
  dimensions, line/arrow angle snapping, and unchanged freehand endpoints;
  zero-length arrow cases also passed.
- The revised panel, select-all highlight, replacement paste, and grid toggle
  were checked through the desktop interface. Shift modifier input itself was
  not manually exercised; its geometry helper was checked directly.
- Dependency clang-tidy checks are now opt-in; project formatting remains checked.

These checks do not constitute a security audit. TLS, per-room authorization,
per-IP/global admission controls, and hostile-network load testing remain future work.
No test folder was added.
