# State machines (2.0)

## Session

`EMPTY → UNKNOWN → HANDSHAKING → READY ↔ BUSY`, or `ERROR`. `ready` stays true during `BUSY` and during a TTL re-handshake of a known device.

## Connect

`connect()` hellos every used slot in parallel, then `miIO.info` if the model is unknown. Failed `TIMEOUT` / `WRONG_DEVICE` slots go to `discover()`. Completion is `CONNECT_DONE`.

## Discovery

Broadcast hello, collect candidates, then each searching slot probes unused candidates with `miIO.info` using that slot's token. Success leaves the slot `READY`. A restart emits `DISCOVERY_DONE(restarted=true)` first.

## Groups

Power, speed, and handshake have separate group slots and deferred entries. `CmdAccept.seq` matches `GROUP_DONE.seq`. `cancel()` emits `GROUP_DONE` for the active wave and for any pre-allocated `nextSeq`.
