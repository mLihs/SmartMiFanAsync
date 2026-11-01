# SmartMiFanAsync

Current version: **2.0.0**

Non-blocking Xiaomi SmartMi fan control over miIO. One UDP socket, one session per slot, application-chosen indices.

```cpp
SmartMiFan::begin(udp);
SmartMiFan::add(0, "192.168.1.50", tokenHex);
SmartMiFan::setEventCallback(onEvent);
SmartMiFan::connect();
// every loop():
SmartMiFan::update();
```

See `docs/06_APIS.md` and `Refactoring/10_V2_DESIGN.md` in the workspace.
