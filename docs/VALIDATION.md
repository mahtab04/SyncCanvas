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

## Offline drawing update (October 31, 2026)

Release desktop/server builds and C++ formatting checks passed. Desktop checks
verified drawing while disconnected, local export/import in an isolated writable
working directory, undo of imported local shapes, and rejection of an additive
1500-shape import while preserving the original local sketch. Room checks verified
that joining an empty room did not upload local work, Add local created one room
shape, and disconnect restored the local original. The review export was restored
after the capacity check. No automated test folder was added.

Local boards are in-memory, not automatically persisted on exit. Export requires a
writable working directory. Long-stroke segmentation and abrupt network-loss paths
were reviewed in code but not manually exercised in this iteration.

## Explicit room selection (November 1, 2026)

The Release client build and formatting checks passed. A plain launch was checked
through the interface: the room field starts empty with a required-name placeholder,
and clicking Join / create room keeps the app in Local Studio, focuses the room
field, and explains the choice. The updated --help output documents the empty default.
CLI --room remains available for users who intentionally preselect a named room.

## UI controls update (November 1, 2026)

The Release client build and all project C++ formatting checks passed. The light
and dark layouts, 12 swatches, custom #22C55E entry/application, and drawing with the
custom color were inspected in the native interface. The review export retained
exact RGB 0x22C55E. Clear confirmation did not alter the drawing; timeout and Escape
canceled it while preserving the sketch. The final local-clear/Undo restore path
was reviewed in code rather than manually executed in this iteration.

An isolated real-TCP check sent ten acknowledged owner-limited undo operations,
verified consecutive revisions, and confirmed another participant's shape remained
with its original RGB value. This verifies the protocol used by Clear mine; the
complete room-clear UI flow was not manually exercised. A dark-mode screenshot is
included in the README. Theme preferences are session-only. Autosave/recovery work
was deferred at the user's request; local work still requires Export before closing.
