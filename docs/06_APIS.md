# Public API (2.0)

Namespace `SmartMiFan`. Include `SmartMiFanAsync.h`.

Lifecycle: `begin(udp)`, `end()`, `update()`, `setHandshakeTtl(seconds)`, `prepareForSleep(closeSocket)`, `wake()`.

Slots: `add(index, ip, token, model)`, `remove(index)`, `clear()`, `used()`, `info(index, out)`, `indexByToken(hex)`.

Connect: `connect(discoveryMs)`, `discover(mask, durationMs)`, `cancel()`, `connecting()`, `discovering()`.

Commands: `setPower` / `setSpeed` / `handshake` return `QUEUED` or `REJECTED`. `setPowerAll` / `setSpeedAll` / `handshakeAll` return `{result, seq}`.

Events: `setEventCallback`. See `EventType` in the header. Callbacks run at the end of `update()` and must not call `update()`.
