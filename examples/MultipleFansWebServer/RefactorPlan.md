# MultipleFansWebServer Refactor Plan (v2)
## Remove JSON from Actions & Harden Stability/Performance

> Ziel: **JSON vollständig aus Actions entfernen**, WebSocket **binär beibehalten**, Settings **robust & atomar** gestalten und **Crash-/Race-Ursachen** in sauberer Reihenfolge beheben.

---

## STEP 0 — Baseline & Inventory

### A) API-Inventar
- Liste **alle** HTTP-Endpunkte (Actions, Settings, State).
- Markiere für jeden Endpunkt:
  - Input-Format (JSON / Query / form-urlencoded)
  - Output-Format (JSON / text/plain / binary via WS)
  - Handler (Datei:Zeile)

### B) JSON-Nutzung
- Finde **alle** ArduinoJson-Stellen.
- Klassifiziere:
  - *server → client Snapshot* (ok)
  - *client → server Parsing* (zu entfernen)

### C) Body-/Concurrency-Analyse
- Identifiziere alle Stellen mit:
  - `request->_tempObject`
  - Body-Chunk-Handling
- Liste alle Call-Sites von:
  - `sendTelemetry()`
  - `sendStateChanged()`
  - `sendProgress()`
- Markiere den Ausführungskontext (Async-Callback / loop / Timer).

**Ziel:** Hotpaths & Async-Kollisionen sichtbar machen.

---

## STEP 1 — Stop the Crash (CRASH FIX FIRST)

### 1.0 Harte Regel
**Actions dürfen keinerlei JSON-Parsing enthalten.**
- Kein `ArduinoJson` in Actions
- Kein `request->_tempObject`
- Keine Body-Chunk-Assembly

### 1.1 Actions ohne JSON
- **Input:** Query-Params **oder** `application/x-www-form-urlencoded`
- **Output:** `text/plain` (`"OK"`, `"BUSY"`, `"ERR:<reason>"`) oder kleine ints
- **HTTP-Codes:** `200`, `400`, `409`, `503`

### 1.2 Scan Action vereinfachen (WICHTIGE KORREKTUR)
Scan ist **long-running** → klare Trennung:

- `POST /api/action/scan/start` → startet Scan
- `POST /api/action/scan/stop` (optional)
- **JobId / Progress / Offers ausschließlich über WebSocket**
- HTTP-Antwort: `"OK"`, `"BUSY"`, `"ERR:<reason>"`

**Begründung:** Deterministisch, reconnect-sicher, weniger UI-Sonderlogik.

### 1.3 Routing
- Query-Params funktionieren ohne Body-Handler.
- Für form-urlencoded: `request->hasParam("x", true)`.

**Ergebnis:** JSON-Crashpfad vollständig entfernt.

---

## STEP 2 — Settings Refactor (No JSON Parsing on ESP)

### 2.0 Grundsatz
**Settings-Writes nur atomar (Key/Value).**  
Kein JSON aus Client-Requests parsen.

### 2A) Atomic Key/Value Settings API
**Endpoints:**
- `GET  /api/settings/get?key=<key>`
- `POST /api/settings/set` (form body: `key=<k>&value=<v>`)
- `GET  /api/settings/list` (text/plain)

**Namensräume (Beispiele):**
- `globalSpeed`
- `fan.0.enabled`
- `telemetry.rate_hz`
- `system.name`

**Persistenz:** Preferences / NVS / LittleFS (projektkonform).  
**Validierung:** Typen + Ranges serverseitig.

### 2B) Config-Datei für große/komplexe Settings (klar begrenzt)
- `POST /api/config/upload`:
  - Speichert Datei **1:1**, **ohne Parser**
  - **Größenlimit** (z. B. 4–16 KB)
  - Optional: CRC/Hash
- `GET /api/config/download`
- `GET /api/config/snapshot`:
  - **JSON nur server → client**
  - Snapshot **read-only**
  - Erzeugt aus internem State/Key-Values
  - **Kein Write-Back per JSON**

---

## STEP 3 — WebUI Updates

### 3.0 Regeln
- **Actions:** `fetch('/api/action/...?...', { method:'POST' })`
- **Responses:** `text()` auswerten (`OK/BUSY/ERR`)
- **Settings:** `POST /api/settings/set` (form-urlencoded)
- **WS:** Binär unverändert

### 3.1 Scan UI
- Start via `/api/action/scan/start`
- Fortschritt/Angebote via **WS**
- Reconnect: `/api/state` Snapshot oder WS-Replay

---

## STEP 4 — Correctness-Fixes (nach Crash)

### 4A) State-Mapping
- Mappe `"SETTINGS_UPDATED"`, `"SPEED_UPDATED"`, `"FAN_ENABLED_CHANGED"` → `STATE_READY`
- (WS-Protokoll bleibt unverändert)

### 4B) Stack-Buffer Hardening
- In `sendStateChanged()` **keine Stack-Buffer** an async send übergeben
- Auf **static/persistente Buffer** umstellen

---

## STEP 5 — Hardening & Performance

### 5A) Telemetry-Serialisierung (WICHTIG)
**Bevorzugt: Dirty-Flag Pattern**
- `markTelemetryDirty()` aus Async-Kontexten
- Senden **nur** aus `loop()`/Timer

**Quickfix (ok):**
- `static volatile bool telemetrySending` Guard

### 5B) Logging
- Verbose Logs nur hinter `#if DEBUG`
- Rate-Limit für Spam-Logs

### 5C) Buffer-Safety
- WS-Frames nur aus statischen/persistenten Buffern senden

### 5D) Bounds & Validation
- `fanIndex` strikt gegen `fanCount` + Array-Grenzen
- Speed/Bool-Parsing robust

---

## STEP 6 — Cleanup & Decommission (NEU)
- Entferne **alle** Legacy-JSON-Action-Endpunkte
- Optional Übergang:
  - `#define API_JSON_LEGACY 0`
- ArduinoJson **nur** noch für server→client Snapshots (oder komplett entfernen)

---

## Summary (v2)
- Actions: **JSON-frei**
- WS: **binär, unverändert**
- Settings: **atomar (Key/Value)** + optional **Datei**
- Scan: **HTTP trigger**, **WS observe**
- Telemetry: **serialisiert**
- Legacy-Pfade: **entfernt**

> Ergebnis: **robuster**, **einfacher**, **ESP-schonender** und **skalierbar**, ohne neue Komplexität.
