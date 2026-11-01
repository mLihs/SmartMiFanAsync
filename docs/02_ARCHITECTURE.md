# Architecture (2.0)

One `SmartMiFan` manager, one `MiioSession` per slot, one UDP socket.

```
SmartMiFan::update()
  poll UDP → route by IP
  timeouts / transparent re-handshake
  advance deferred commands, groups, discovery, connect
  dispatch event queue
```

Slot index is chosen by the application (`add(index, …)`). Masks are `uint8_t` (`SMARTMIFAN_MAX_FANS` ≤ 8). No heap in the library. `info()` / `participation()` may be read from any task; mutating calls run on the `update()` task.
