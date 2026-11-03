# Security and deployment scope

SyncCanvas is intended for local development and trusted networks. The standalone server defaults to 127.0.0.1. Desktop Start server listens on all IPv4 interfaces (0.0.0.0) by default.

The desktop's Start server uses the same server and security limits and accepts LAN
connections by default. There is no LAN checkbox. The optional Access key
field configures the embedded server before startup and remains fixed until Stop.
Hosting does not alter firewall rules. Closing the host window ends all its sessions.
Embedded archives live in a per-user, per-port directory separate from standalone
archives; do not run another writer against it. Stop reports final save failures,
but still shuts down, so storage health and backups remain the operator's responsibility.

This version has **no TLS, user accounts, per-room passwords, or access-control lists**. An optional server-wide shared access key gates the join operation. Without a configured key, anyone able to connect can join any known room name, read its board, and append shapes. With a key, every key holder can still access every room. Display names are not verified identities. Undo is restricted to shapes owned by the current connection, but this is a session rule rather than account-level authorization.

Do not expose the listener directly to the public internet or store sensitive content in rooms. Use a controlled network boundary while evaluating it. An internet deployment requires authenticated sessions, per-room authorization, encryption, abuse controls appropriate to the environment, and operational testing.

Implemented safeguards include validated frame lengths, bounded JSON nesting, shape/coordinate limits, per-connection rate limits, queue caps, connection deadlines, and room identifiers that cannot contain path separators. These controls are not a security audit or a denial-of-service guarantee.

Persistence logs failures to stderr and retries dirty boards. Operators must monitor logs, protect the data directory, back it up, and run only one writer per directory. Power-loss durability and recovery from disk exhaustion are not guaranteed. A crash may lose edits since the last successful autosave.

GitHub Actions configuration is included, but successful hosted CI and cross-platform deployment must be checked in the actual repository before release.

## Optional shared access key

Set `SYNCCANVAS_ACCESS_KEY` in the server environment before startup to require it.
Use a randomly generated key containing 16-128 ASCII letters, digits, underscores,
or hyphens. An invalid configured value stops startup rather than opening the server.
The desktop reads the same environment variable, or accepts the key in its masked
Access key field. Keys are not written to board archives or included in server logs.
Local `.env` files are ignored by Git, but the application does not load them itself.

The key is sent in the TCP join message. **It is not encrypted and does not authenticate
the server.** It can be intercepted or replayed by an attacker on the network. Use this
feature only on loopback or inside a trusted, encrypted network boundary. Do not reuse
an account password as the key. Restart the server with a new key to rotate it; existing
sessions end when the server stops. This feature does not make public hosting safe.
See [OWASP transport guidance](https://cheatsheetseries.owasp.org/cheatsheets/Transport_Layer_Security_Cheat_Sheet.html).

## Admission and resource controls

A successful join must complete within 10 seconds of acceptance; sending other
messages does not extend that deadline. Existing per-peer operation limits remain.
Incoming connections also have a 512 KiB/second byte budget with a 2 MiB burst,
charged against declared frame sizes before allocating/reading their bodies. Whitespace
and ignored JSON fields count toward this limit. Client snapshot reception is not
subject to that inbound-client budget. These are per-connection controls, not global
or per-IP protection against a distributed attack.

Malformed operation fields receive a generic error. Archive failures do not expose
local file paths or parser details to peers. The standalone executable retains its loopback default.

## Local recovery storage

Local autosave stores validated drawing geometry in per-user recovery directories,
separate from hosted room archives. Files are plaintext, without access keys or
connection settings. Protect the OS user account and use Export for backups. Each
live window holds an OS lock on its slot; this prevents cooperating app instances
from overwriting each other's local work, but is not an access-control boundary.
Unreadable files are preserved rather than overwritten. A failed close save is
reported and keeps the window open unless the user explicitly exits without saving.
