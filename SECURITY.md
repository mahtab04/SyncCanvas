# Security and deployment scope

SyncCanvas is intended for local development and trusted networks. The default server bind is 127.0.0.1. Binding to another interface is an explicit administrator choice.

This version has **no TLS, authentication, room passwords, or access-control lists**. Anyone able to connect can join any known room name, read its board, and append shapes. Display names are not verified identities. Undo is restricted to shapes owned by the current connection, but this is a session rule rather than account-level authorization.

Do not expose the listener directly to the public internet or store sensitive content in rooms. Use a controlled network boundary while evaluating it. An internet deployment requires authenticated sessions, per-room authorization, encryption, abuse controls appropriate to the environment, and operational testing.

Implemented safeguards include validated frame lengths, bounded JSON nesting, shape/coordinate limits, per-connection rate limits, queue caps, connection deadlines, and room identifiers that cannot contain path separators. These controls are not a security audit or a denial-of-service guarantee.

Persistence logs failures to stderr and retries dirty boards. Operators must monitor logs, protect the data directory, back it up, and run only one writer per directory. Power-loss durability and recovery from disk exhaustion are not guaranteed. A crash may lose edits since the last successful autosave.

GitHub Actions configuration is included, but successful hosted CI and cross-platform deployment must be checked in the actual repository before release.
