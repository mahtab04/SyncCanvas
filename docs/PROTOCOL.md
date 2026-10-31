# Wire protocol v1

Transport is TCP, not HTTP or WebSocket. Each message is a four-byte unsigned big-endian byte length followed by a UTF-8 JSON object. Zero-length frames and frames larger than 524,288 bytes are rejected before body allocation. The JSON object must have a string `type`. JSON nesting is limited to 16.

## Client to server

~~~json
{"type":"join","version":1,"room":"studio","name":"alice","access_key":"optional-server-key"}
{"type":"draw","shape":{"kind":"rect","color":4644797,"width":3,"points":[[10,20],[100,200]]}}
{"type":"preview","shape":{"kind":"pen","color":4644797,"width":3,"points":[[10,20],[20,30]]}}
{"type":"preview","shape":null}
{"type":"undo"}
{"type":"cursor","point":[45,60]}
{"type":"ping"}
~~~

A connection must successfully join within 10 seconds of being accepted and joins once. A server configured with `SYNCCANVAS_ACCESS_KEY` validates the optional `access_key` string before loading a room or returning board data; absent or wrong keys receive an access-denied error. Open servers ignore this field. Keys travel in plain TCP. Room IDs have 1–32 ASCII alphanumeric, hyphen, or underscore characters; names have 1–24. Other operations require a successful join. Unsupported protocol versions are rejected.

Shape kind is pen, rect, diamond, ellipse, arrow, or line. Color is an RGB integer in [0, 16777215]. Width is finite and in [1, 16]. Coordinates are finite and bounded to a 1200 by 760 logical board. Pen shapes contain 2–512 points; all other shapes contain exactly two points. Rectangles, diamonds, and ellipses use opposite bounding-box corners; lines and arrows use their start and end points. The extra shape kinds are an additive v1 extension requiring updated peers. Supplied ID/owner fields are ignored; identity always comes from the server.

## Server to client

| Type | Fields | Meaning |
|---|---|---|
| welcome | version, id, room, access_controlled | Accepted connection identity |
| snapshot_begin | revision | Start a replacement board snapshot |
| snapshot_shape | shape | One complete shape with id and owner |
| snapshot_end | revision | Atomically activate the assembled snapshot |
| added | shape, revision | One accepted drawing operation |
| removed | id, revision | One accepted undo |
| peer_join | id, name | Participant present; idempotent map update |
| peer_leave | id | Participant disconnected |
| cursor | id, point | Transient cursor position |
| preview | id, shape | Replace participant preview; null clears it |
| pong | — | Heartbeat response |
| error | message | Rejected application-level operation |

Revision increases exactly once per successful board insertion/removal. An undo with no owned shape is a no-op and emits no board event. Cursor, preview, and presence events never consume revisions. Preview geometry uses the same validation and point limits as committed shapes. The server supplies the participant ID and ignores supplied identity fields. Each peer has one transient preview, relayed only within its room; it is cleared before a draw is accepted or rejected, on an explicit null preview, or when that peer leaves. New joiners receive active previews after the board snapshot and corresponding peer_join messages. Desktop clients throttle preview updates to 20 per second. Preview messages are an additive v1 extension; both server and clients must be updated to use live drawing. Peers receive their own accepted edits so all visible committed shapes follow the same path.

Incoming client frames are also limited to 512 KiB/second with a 2 MiB burst. Bytes are charged before body allocation, including whitespace and ignored fields. Snapshot messages are separate bounded frames; a full board is never forced into a single large JSON frame. Unknown/malformed framing or JSON closes the connection. Well-formed but invalid application operations receive error messages. A connection exceeding its rate limit or outgoing queue capacity is closed.

## Archive format

~~~json
{"format":"synccanvas","version":1,"shapes":[{"kind":"rect","color":4644797,"width":3,"points":[[10,20],[100,200]]}]}
~~~

Saved documents may include IDs/owners, but those are not trusted when loading. Room imports and Add local validate capacity before queueing and send ordinary draw operations; they are additive and are not transactional network batches. Offline drawing and offline import/undo do not send protocol operations. Local imports validate the combined board transactionally before replacing local state.
