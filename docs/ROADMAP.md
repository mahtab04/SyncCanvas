# Product direction

SyncCanvas is a drawing studio: sketch locally immediately, explicitly share a copy
into a room, and return to the shared board later. The current native client prioritizes live geometry and a
small set of predictable tools.

## Implemented in this iteration

- Server/port share a row; room, name, and optional access key remain easy to find.
- Desktop hosting has Start/Stop, stop confirmation, and LAN access enabled by default; no separate executable is needed.
- Connection fields support select-all, validated paste, and keyboard traversal.
- Shift constrains geometry; the dot grid can be hidden without changing the board.
- Light/dark themes, 12 preset colors, and custom RGB hex entry are available.
- Local clear has confirmation and Undo recovery; room clearing is limited to current-session ownership.
- A configured server key gates room access, with a masked field and explicit
  indication that the transport is still unencrypted.
- Join deadlines, inbound byte limits, and generic operation errors reduce common
  resource-exhaustion and accidental-information-disclosure paths.

## Offline drawing

Local drawing, local undo, transactional additive import, export, and explicit Add
local copying are implemented. Local boards survive room switches in memory. Local
autosave and recovery are implemented with per-window locking, background atomic
saves, failure reporting, and a final save on close.

## Next: editing and navigation

Add selection, move/resize, text labels, and redo. Keep draft geometry separate from
committed shapes and apply changes through the authoritative server. Add zoom/pan
and a fit-board action, with consistent hit testing at each zoom level. Then support
fill and stroke styles with a versioned document schema and backward compatibility.

## Next: secure collaboration

Before public hosting, add TLS with server certificate validation, authenticated
identities, and per-room viewer/editor roles. Replace the server-wide shared key
with revocable, expiring invitations or authenticated sessions. Bind undo ownership
to the authenticated identity. Add per-IP/global admission controls and inspect
hosted security checks. A shared key over plain TCP is not this security model.

## Next: durability and release quality

Move persistence off the networking loop, add a durable operation journal and
recovery checks, and define backup/restore behavior. Introduce a version-negotiation
policy before incompatible protocol changes. Verify Windows and Linux release
builds in CI and exercise malicious-peer and slow-client scenarios before release.

Automated test sources remain outside this repository at the user's request;
one-off checks and their practical limits are documented in VALIDATION.md.
