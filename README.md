# SmartMiFanAsync

Current version: **2.0.1**

Non-blocking Xiaomi / Smartmi fan control over miIO. One UDP socket, one session per slot, application-chosen indices.

```cpp
SmartMiFan::begin(udp);
SmartMiFan::add(0, "192.168.1.50", tokenHex);
SmartMiFan::setEventCallback(onEvent);
SmartMiFan::connect();
// every loop():
SmartMiFan::update();
```

## Supported models (Homewind)

- Xiaomi Smart Standing Fan 2 Pro (`dmaker.fan.p33`)
- Xiaomi Smart Standing Fan 2 (`dmaker.fan.p30`)
- Xiaomi Smart Standing Fan Pro Slim (`xiaomi.fan.p85`)
- Smartmi Standing Fan 3 (`zhimi.fan.za5`)
- Xiaomi Smart Standing Air Circulation Fan
- Xiaomi Smart Desktop Air Circulation Fan (`xiaomi.fan.p70`)

Full model table and speed parameters: `docs/01_PROJECT_OVERVIEW.md`. API: `docs/06_APIS.md`.
