# Examples (2.0)

All sketches call `SmartMiFan::begin`, `update()` every `loop()`, and use events instead of `is*Complete` polling.

## Canonical six

| Sketch | What it shows |
|---|---|
| `BasicDiscovery` | `add` + `discover(mask)` |
| `Connect` | known IP + `connect()` |
| `GroupCommands` | `setSpeedAll` + `GROUP_DONE.seq` |
| `Participation` | `setEnabled` / `participating()` |
| `Events` | `EventCallback` |
| `WebServer` | HTTP status + group command |

## Also ported from 1.x

`BasicAsyncDiscovery`, `AsyncQueryDevice`, `MultipleFansAsync`, `MultipleFansFastConnect`, `MultipleFansSmartConnect`, `IndependentFansControl`, `FanParticipationToggle`, `ErrorAndHealthCallbackExample`, `SystemStateOrchestration`, `FullOrchestrationFlow`, `WebServerControl`, `MultipleFansWebServer`, `HeapFragmentationTest`.

`SystemState` is gone: sleep/wake is `prepareForSleep` / `wake`. Health checks are `handshake` / `handshakeAll`.
