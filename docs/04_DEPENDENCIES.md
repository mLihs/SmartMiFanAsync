# SmartMiFanAsync - Externe Abhängigkeiten

**Version**: 1.8.2  
**Zielplattform**: ESP32 (Arduino Core)  
**Letzte Aktualisierung**: 2026-01-16

---

## Übersicht

Diese Dokumentation listet alle externen Bibliotheken auf, die von SmartMiFanAsync verwendet werden.

---

## Externe Libraries

### ESP32 Arduino Core (erforderlich)

| Library | Header | Verwendung |
|---------|--------|------------|
| **Arduino** | `<Arduino.h>` | Arduino-Framework Basisfunktionen (`millis()`, `Serial`, etc.) |
| **WiFi** | `<WiFi.h>` | WiFi-Konnektivität, `IPAddress` Klasse |
| **WiFiUdp** | `<WiFiUdp.h>` | UDP-Kommunikation für miIO-Protokoll (Port 54321) |

### mbedTLS (im ESP32 Arduino Core enthalten)

| Library | Header | Verwendung |
|---------|--------|------------|
| **AES** | `"mbedtls/aes.h"` | AES-128-CBC Verschlüsselung/Entschlüsselung für miIO-Protokoll |
| **MD5** | `"mbedtls/md5.h"` | MD5-Hashing für Key/IV-Ableitung aus Device-Token |

### C Standard Library

| Library | Header | Verwendung |
|---------|--------|------------|
| **string** | `<string.h>` | String-Manipulation (`strlen`, `memcpy`, `strncmp`, etc.) |

---

## Detaillierte Beschreibungen

### Arduino.h

Basis-Framework für Arduino-Programmierung auf ESP32.

**Verwendete Funktionen:**
- `millis()` - Zeitstempel für Timeouts und State Machines
- `Serial.printf()` - Debug-Ausgaben (wenn aktiviert)
- `delay()` - Nur in Blocking-Operationen (z.B. Handshake)

### WiFi.h / WiFiUdp.h

UDP-Kommunikation für das miIO-Protokoll.

**Verwendete Klassen:**
- `WiFiUDP` - UDP Socket für Senden/Empfangen
- `IPAddress` - IP-Adress-Handling

**Verwendete Methoden:**
```cpp
WiFiUDP udp;
udp.begin(port);
udp.beginPacket(ip, port);
udp.write(buffer, length);
udp.endPacket();
udp.parsePacket();
udp.read(buffer, length);
udp.remoteIP();
udp.stop();
```

### mbedTLS (AES / MD5)

Kryptographie für das miIO-Protokoll.

**AES-128-CBC Verschlüsselung:**
```cpp
#include "mbedtls/aes.h"

mbedtls_aes_context aes;
mbedtls_aes_init(&aes);
mbedtls_aes_setkey_enc(&aes, key, 128);
mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, iv, input, output);
mbedtls_aes_free(&aes);
```

**MD5 Hashing:**
```cpp
#include "mbedtls/md5.h"

mbedtls_md5_context ctx;
mbedtls_md5_init(&ctx);
mbedtls_md5_starts(&ctx);
mbedtls_md5_update(&ctx, input, length);
mbedtls_md5_finish(&ctx, output);
mbedtls_md5_free(&ctx);
```

**Key/IV-Ableitung aus Token:**
```
Key = MD5(token)
IV  = MD5(Key + token)
```

---

## Abhängigkeitsdiagramm

```
SmartMiFanAsync
├── Arduino.h          (ESP32 Arduino Core)
├── WiFi.h             (ESP32 Arduino Core)
├── WiFiUdp.h          (ESP32 Arduino Core)
├── string.h           (C Standard Library)
└── mbedtls/           (Teil von ESP32 Arduino Core)
    ├── aes.h          (AES-128-CBC)
    └── md5.h          (MD5 Hashing)
```

---

## Installation

### Keine zusätzlichen Installationen erforderlich!

Alle Abhängigkeiten sind im **ESP32 Arduino Core** enthalten. Nach Installation des ESP32 Board Package sind alle Libraries verfügbar.

**Arduino IDE:**
1. Board Manager → ESP32 by Espressif Systems installieren
2. SmartMiFanAsync Library installieren
3. Fertig!

**PlatformIO:**
```ini
[env:esp32]
platform = espressif32
framework = arduino
lib_deps = 
    SmartMiFanAsync
```

---

## Kompatibilität

| ESP32 Arduino Core Version | Status |
|---------------------------|--------|
| 2.0.x | ✅ Kompatibel |
| 3.0.x | ✅ Kompatibel |

### Hinweise zur Kompatibilität

- **mbedTLS API**: Die verwendeten mbedTLS-Funktionen sind in allen ESP32 Arduino Core Versionen verfügbar
- **WiFiUdp**: Stabile API seit ESP32 Arduino Core 1.0

---

## Optionale Libraries

### DebugConfig.h (Optional)

Falls vorhanden, wird diese Header-Datei automatisch inkludiert:

```cpp
#if defined(__has_include)
  #if __has_include("DebugConfig.h")
    #include "DebugConfig.h"
  #endif
#endif
```

Ermöglicht projekt-spezifische Debug-Konfiguration ohne Library-Änderungen.

---

## Siehe auch

- [01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md) - Projektübersicht
- [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) - Architektur und miIO-Protokoll
- [06_APIS.md](./06_APIS.md) - API-Referenz
