# ESP32 Hologram Controller V2

A complete ESP32-based hologram / LED pattern controller driven by two cascaded **74HC595** shift registers (16 LEDs), with a WiFi web interface for real‑time pattern editing, IR‑triggered playback, and start delay.

**Live site:** <https://devshehan6.github.io/ESP32-HOLOGRAM-V2/>

---

## 🔗 Quick Links

| Page | Purpose | Link |
|---|---|---|
| 🏠 **Connect** | Enter the ESP32 IP address to log in | [Open ↗](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/index.html) |
| 🧪 **Test Panel** | Test bulbs one-by-one and pass LED count to Frame editor | [Open ↗](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/test.html) |
| 🎬 **Frame Editor** | Build frame patterns, set delays, send to ESP32 | [Open ↗](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/frame.html) |

👉 **Start here:** [Open the Connect page](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/index.html)

---

## ✨ Features

- **16-LED output** via 2 × 74HC595 cascaded shift registers
- **WiFi web control** — no app needed, works from any browser
- **Three-page workflow** — Connect → Test → Frame Editor
- **Live online/offline bulb** on both test and frame pages
- **IR-triggered playback** — pattern plays only when IR signal is received
- **Start Delay** — configurable wait time (ms / s) after IR trigger before frame starts
- **Frame Delay** — configurable time between lines
- **Single-shot playback** — frame runs once, then waits for next IR trigger
- **Multi-frame editor** with pagination (`<< < [1][2][3] > >>`)
- **Dynamic rows and lines** — add/remove as needed
- **Local storage persistence** — all settings survive page reloads
- **Export / Import** — save and restore frames as JSON
- **Mobile-friendly UI** — works on phone screens
- **WiFi status LED** on the ESP32 built-in LED
- **Remote-style operation** — web only sends data; the board keeps running even when the browser is closed

---

## 🧰 Hardware

| Component | Qty |
|---|---|
| ESP32 DevKit (or compatible) | 1 |
| 74HC595 shift register | 2 |
| LEDs (with resistors) | 16 |
| IR receiver (VS1838B / TSOP1838) | 1 |
| Jumper wires + breadboard | — |

### Pin Mapping

| Function | ESP32 GPIO | Notes |
|---|---|---|
| Shift register DATA (DS) | GPIO 0 | 74HC595 pin 14 |
| Shift register CLOCK (SHCP) | GPIO 1 | 74HC595 pin 11 |
| Shift register LATCH (STCP) | GPIO 2 | 74HC595 pin 12 |
| IR receiver OUT | GPIO 4 | VS1838B / TSOP1838 |
| Built-in LED | GPIO 2 (default) | Change if your board differs |

> ⚠️ If your board's built-in LED isn't on GPIO 2, update `LED_BUILTIN_PIN` in the firmware.

### Cascade Wiring

- Q7' (pin 9) of chip 1 → DS (pin 14) of chip 2
- Both chips share SHCP and STCP lines
- Arduino `DATA_PIN` → DS of chip 1

---

## 🌐 Web Pages

### 1. `index.html` — Connect
- Manual IP entry (no auto-detection)
- Saves the last successful IP to `localStorage`
- Pings `/ping` before redirecting to `test.html`

### 2. `test.html` — Test Panel
- 2 default rows × 8 switches
- Add / remove rows dynamically
- **Live** toggle — auto-send switch changes to the ESP
- **Send** — manual send of current state
- **Next →** — passes LED count (`rows × 8`) to `frame.html`
- Per-row and global **All On / All Off**
- Online/offline bulb (top right)

### 3. `frame.html` — Frame Editor
- **Frame Delay** (per-line timing) with ms/s dropdown
- **Start Delay** (wait time after IR trigger) with ms/s dropdown
- Single horizontal LED strip per line for easy hologram visualization
- Multi-frame editor with pagination
- Bottom action bar:
  - `Send` — send current frame to ESP
  - `Stop` — halt playback, all LEDs off
  - `Export` / `Import` — JSON file save/load
  - `Add Frame` / `Add Frame + Old Data`
  - `Remove Frame` / `Clean Frame` / `Delete All Frames`

---

## 🔄 End-to-End Workflow

1. **Connect** — open `index.html`, enter ESP32 IP, click **Connect**
2. **Test** — on `test.html`, toggle switches to verify each LED works
3. **Next** — click **Next →** to open the Frame Editor
4. **Design** — build lines (each line = one LED pattern), add frames
5. **Configure** — set **Frame Delay** (line duration) and **Start Delay** (post-IR wait)
6. **Send** — click **Send** to arm the ESP
7. **Trigger** — the ESP turns LEDs off and waits for an **IR signal**
8. **Playback** — on IR signal, wait `Start Delay`, then run through the frame's lines once
9. **Repeat** — after the frame finishes, LEDs go off and the ESP waits for the next IR signal

---

## 🔌 ESP32 Endpoints

| Endpoint | Method | Purpose |
|---|---|---|
| `/ping` | GET | Connectivity check (`{"ok":1}`) |
| `/status` | GET | Current state / line count |
| `/set?value=N` | GET | Immediately set 16-bit output (used by test page) |
| `/frames` | POST | Load a frame; arm for IR trigger |
| `/stop` | GET | Stop playback, LEDs off |

### `/frames` payload

```json
{
  "delay": 100,
  "startDelay": 2000,
  "lines": ["0000000011111111", "1111000011110000"]
}
```

- `delay` — time (ms) per line
- `startDelay` — wait time (ms) after IR trigger before frame begins
- `lines` — array of 16-bit binary strings (MSB = LED 15, LSB = LED 0)

---

## 🎯 IR Trigger Behaviour

- Firmware sits in `TS_WAITING_IR` state after a frame is loaded
- LEDs are off during waiting
- On IR signal:
  - Record time → enter `TS_START_DELAY`
- After `startDelay` elapses:
  - Enter `TS_PLAYING`, show line 1
- Every `frameDelay` ms:
  - Advance to the next line
- After the last line:
  - LEDs off → back to `TS_WAITING_IR`
- Next IR signal restarts the whole cycle

A built-in **300 ms cooldown** ignores IR repeat codes so one button press triggers only once.

---

## 💾 Local Storage Format

`localStorage["shiftReg_frameDB"]`:

```json
{
  "delayValue": 100,
  "delayUnit": "ms",
  "startDelayValue": 2000,
  "startDelayUnit": "ms",
  "ledCount": 8,
  "currentFrame": 0,
  "frames": [
    [ [0,0,0,0,0,0,0,0], [1,1,1,1,0,0,0,0] ],
    [ [1,0,1,0,1,0,1,0] ]
  ]
}
```

| Key | Purpose |
|---|---|
| `shiftReg_deviceIP` | Last successful ESP32 IP |
| `shiftReg_ledCount` | LEDs passed from test page |
| `shiftReg_frameDB` | Full frame database |

---

## 🚀 Setup Instructions

### 1. ESP32 Firmware

1. Open Arduino IDE
2. Install the ESP32 board package
3. Create a new sketch, paste the firmware
4. Update WiFi credentials:
   ```cpp
   const char* WIFI_SSID = "YOUR_WIFI";
   const char* WIFI_PASS = "YOUR_PASSWORD";
   ```
5. Upload to the ESP32
6. Open Serial Monitor at **115200 baud**
7. Note the printed IP address

### 2. Web Pages

Pages are hosted on GitHub Pages — just open:

👉 <https://devshehan6.github.io/ESP32-HOLOGRAM-V2/>

### 3. HTTPS Caveat

GitHub Pages serves over **HTTPS**, but the ESP32's web server runs over **HTTP**. Browsers block mixed-content requests by default. To work around this:

- **Option A** — open the HTML files locally (`file://`)
- **Option B** — serve the ESP32 over HTTPS with a self-signed cert
- **Option C** — host the pages on a plain HTTP static server

---

## 🛠 Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `Send failed: HTTP 400` | Body missing / wrong CORS | Use `Content-Type: text/plain`, register body handler |
| Bulbs don't respond | Wrong endpoint | Ensure firmware has `/set` handler |
| Online bulb always offline | Browser blocking HTTP from HTTPS page | Open pages locally |
| Pattern doesn't play after Send | No IR signal received yet | Send IR signal (this is by design) |
| Frame runs in a loop | Firmware uses looping `loop()` | Use single-shot `TS_PLAYING` version |
| IR triggers multiple times | Repeat codes | Increase `irCooldown` (300 ms default) |
| Built-in LED not lighting | Wrong pin | Set `LED_BUILTIN_PIN` to your board's LED GPIO |
| LEDs 16+ don't respond | Firmware only handles 16 bits | Upgrade to `uint32_t` and 4-byte shiftOut |

---

## 📁 Project Structure

```
ESP32-HOLOGRAM-V2/
├── index.html      # Connect page
├── test.html       # Test panel with switches
├── frame.html      # Frame editor
└── README.md       # This file
```

---

## 📋 Operation Quick Reference

| Action | How |
|---|---|
| Connect to ESP32 | Open `index.html`, enter IP, click Connect |
| Test a bulb | Toggle a switch in `test.html`, click Send |
| Open frame editor | From `test.html`, click **Next →** |
| Add a line | Click **+ Add Line** or a line's **Add Line** chip |
| Add a frame | Click **Add Frame** (blank) or **Add Frame + Old Data** (copy) |
| Set timing | Frame Delay + Start Delay in the top bar |
| Send to ESP | Click **Send** |
| Halt playback | Click **Stop** |
| Save your work | Click **Export** (JSON download) |
| Restore your work | Click **Import**, choose a JSON file |

---

## 🧠 How the Board Behaves Without the Browser

The web interface is a **remote control only**:

- Once you press **Send**, the ESP32 stores the frame in memory
- The frame plays when IR triggers it, regardless of whether the browser is open
- Closing the browser or losing WiFi does **not** stop the board
- The online bulb simply indicates connectivity — the board keeps running
- To stop playback, send **Stop**, or press the ESP32's reset button

---

## 📝 License

Personal project — feel free to use and modify.

---

## 👤 Author

**Shehan** — [@devshehan6](https://github.com/devshehan6)

Project repo: <https://github.com/devshehan6/ESP32-HOLOGRAM-V2>

---

## 🔗 Quick Navigation

- 🏠 [Connect Page](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/index.html)
- 🧪 [Test Panel](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/test.html)
- 🎬 [Frame Editor](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/frame.html)
- 💻 [GitHub Repository](https://github.com/devshehan6/ESP32-HOLOGRAM-V2)
- 🌐 [Live Site](https://devshehan6.github.io/ESP32-HOLOGRAM-V2/)
