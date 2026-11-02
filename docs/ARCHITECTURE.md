# Architecture

## Ownership and class responsibilities

The server owns one Asio event loop. Its peer map owns each Connection, and its room map owns Board instances. All operations on these objects occur on the server's event-loop thread. No room mutex is needed because mutation is serialized on that thread.

The desktop Application owns NetworkClient, ClientView, ClientLayout, and Renderer. The GLFW/OpenGL thread alone handles input, view state, and rendering. NetworkClient owns a second event loop and a worker thread; UI requests are posted to that loop, and received JSON events cross back through a mutex-protected mailbox. Sockets never run on the rendering thread.

Application also owns HostedServer, which creates the same Server implementation
used by the standalone executable on a separate worker thread. Bind/listen completes
before Start reports success. Embedded servers do not register process signal
handlers. Stop posts a final save and event-loop shutdown, joins the worker, and
destroys its sockets; window shutdown follows the same path. Final save failures
are reported, but stopping still releases the listener. HostedServer controls only
its own instance, never an external process. Desktop hosting listens on all IPv4 interfaces by default; the standalone
executable retains its loopback default. Per-user, per-port hosted archives are separate
from the standalone server's configured directory. Only room/name remain editable
while hosting offline; joining still uses the normal TCP snapshot path.

Connection owns the socket, timeout timer, frame buffers, and write queue. Shared ownership keeps buffers alive until pending handlers finish. Close is idempotent, cancels I/O, and posts its notification to avoid reentrant peer-map erasure during broadcast.

Board encapsulates shape insertion, server-assigned identity, revision updates, owner-limited undo, and validated document loading. A temporary Board is validated before loading replaces an existing document. Search-style algorithm inheritance is unnecessary here: each class has one concrete responsibility and communicates through explicit data contracts.

## Synchronization

1. The client resolves and connects asynchronously, with an eight-second connection deadline.
2. It sends protocol version, name, room ID, and the optional server access key. The server validates a configured key before touching room archives. Each accepted connection has a non-renewable 10-second successful-join deadline.
3. The server sends welcome, snapshot_begin, bounded snapshot_shape messages, and snapshot_end.
4. The client builds the snapshot separately and swaps it into the visible board on completion.
5. A draw request carries geometry and style only. The server validates it, assigns an ID and owner, increments revision, and broadcasts the accepted shape to the entire room, including the author.
6. Clients verify consecutive revisions. A mismatch triggers disconnect and resynchronization.

TCP provides ordering per connection. Because the server handles each operation on one loop, it provides a single total edit order per room. Snapshots are queued in that same loop before subsequent edits. No CRDT is used: the server is authoritative, and the supported shared operation is shape append/removal.

Live cursors and drawing previews are transient and unversioned. While dragging, clients send validated full-shape previews at most 20 times per second. Each participant has at most one preview (up to 512 points); a new preview replaces it. The server relays previews within the room and includes active previews after the snapshot/presence sequence for new joiners. Complete shapes are durable. Committing a draw clears its preview before broadcasting the accepted shape; cancellation clears it explicitly, and disconnect removes it with the participant. Previews are excluded from persistence, exports, undo, and board revisions. Network errors discard local drafts/import queues and trigger a fresh snapshot after reconnection; there is no exactly-once replay of pending requests.

## Resource limits and failure paths

- A four-byte big-endian header is validated before allocating the body; the maximum is 512 KiB.
- JSON nesting is limited to 16 before parsing; shapes accept only supported tools, finite coordinates, bounded stroke widths, and bounded point counts.
- Each transport queue is capped at 4 MiB and 4096 messages. A slow receiver is disconnected rather than retaining unbounded writes.
- The UI mailbox has equivalent byte/message limits. Overflow forces reconnection.
- A byte token bucket on accepted transports limits declared incoming frame sizes to 512 KiB/second with a 2 MiB burst before allocation, including padded messages. Client snapshot reception stays unlimited by this bucket.
- A token bucket limits each connection to 120 messages/second, burst 240. Cursor updates are generated at most 20 times/second by the desktop client.
- A connection must keep sending complete messages; the transport closes it after 35 seconds without one. Connected clients ping every ten seconds.
- Room files are restricted to validated room IDs under the configured data directory. File size, board count, shape count, and geometry budgets are checked.

The client retries connection loss after three seconds. The disconnect button stops retrying. A join validation error is shown and requires explicit correction rather than repeated join attempts.

## Persistence

Dirty rooms are written every two seconds. Server autosaves and desktop exports share the DocumentFile helper. JSON is serialized before touching the destination, then written, flushed, and closed in a uniquely created temporary directory beside the target. The completed file replaces the previous file (MoveFileExW on Windows, filesystem rename on POSIX). Temporary files and directories are cleaned up on success or failure. Concurrent saves use separate staging directories; the last successful replacement wins. A failed save leaves the room dirty and is logged for retry. This prevents ordinary partial-file replacement, but does not promise power-loss durability on every filesystem. There is no write-ahead journal or replication.

Room data is loaded lazily on the first join. Both room loading and desktop import enforce the 16 MiB limit during chunked reads, rather than relying on a file-size check before reading. Open and read errors are reported explicitly. Malformed archives do not replace a live board. Persisted identity is intentionally not an authentication scheme: restored shapes become archive-owned and cannot be undone by a new session.

## Extending the app

Add new protocol operations in Protocol validation, Board model rules, Server dispatch, and client event handling together. Verify model invariants and replication behavior when changing these operations. Future authenticated identity must replace connection-based ownership; public hosting also needs TLS, authorization, operational monitoring, and a persistence worker/journal.

## Local and room boards

Application owns a separate Board for local work, using the same geometry, shape,
and point limits as the server. Disconnected input commits locally with local
ownership, and local undo removes the latest local shape. Local imports build a
combined candidate Board first, so an invalid or over-capacity import changes nothing.

Connecting keeps that Board in memory while a server snapshot replaces only the
visible room state. Disconnect restores the local shapes and revision. Room snapshots,
errors, and reconnects never overwrite the local Board. Drawing and import/export are
paused during the connection/retry phase; Disconnect resumes local editing.

Add local copies local shapes into the existing throttled import queue. It sends
ordinary draw operations, with fresh IDs and ownership assigned by the server. This
is an explicit additive copy, not automatic merge/replay. Interruption can leave a
partial room copy, and repeating the action can duplicate shapes. Local originals
remain unchanged. Export saves whichever board is visible using atomic replacement;
local work is otherwise memory-only and must be exported before closing.

## Explicit room selection

A plain desktop launch leaves the room name empty. Join attempts with an empty or
invalid room name focus the room field and return before resetting the local view
or starting network work. A valid name is an explicit join-or-create request;
`--room ROOM` may prefill it for scripted launches. Existing names restore their
saved room boards, while new names start empty. The server never selects a room
for a client, and independent launches do not inherit a previous room implicitly.

## Theme, color, and clear controls

Theme changes affect rendering only. Very dark geometry gets a contrast underlay
on dark paper without copying its point buffer or changing stored RGB/style values.
The palette has 12 presets; the custom color field accepts six validated hex digits
with an optional leading # on paste. Applying a color does not connect to a room,
and drawing shortcuts are suppressed while the color field is being edited.

Clear requires a second click within five seconds. Local clear keeps the previous
Board as one bounded in-memory backup, recoverable with Undo. A later local clear
replaces that backup. Shared Clear mine sends existing owner-limited undo operations
at most 40 times per second, with one outstanding acknowledgement at a time. It
accounts for late accepted shapes, blocks simultaneous drawing/imports, and stops
on errors or disconnect. Other owners' shapes and archived shapes are not cleared.
No protocol extension or server-wide destructive clear operation was introduced.
