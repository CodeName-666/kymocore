# 3D protocol status

The former JSON 3D extension was incorporated into Kymotrace Compact Binary
Protocol v6. X and Z presence are descriptor flags, and `send3D()` emits a
17-byte frame or a 21-byte frame with timestamp.

See `../PROTOCOL.md` for the authoritative format and golden vectors. Existing
JSON 3D messages remain accepted by KymoStudio as a legacy
receive format.
