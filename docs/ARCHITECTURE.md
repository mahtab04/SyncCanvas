# Architecture

## Ownership and class responsibilities

The server owns one Asio event loop. Its peer map owns each Connection, and its room map owns Board instances. All operations on these objects occur on the server's event-loop thread. No room mutex is needed because mutation is serialized on that thread.

The desktop Application owns NetworkClient, ClientView, ClientLayout, and Renderer. The GLFW/OpenGL thread alone handles input, view state, and rendering. NetworkClient owns a second event loop and a worker thread; UI requests are posted to that loop, and received JSON events cross back through a mutex-protected mailbox. Sockets never run on the rendering thread.

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
