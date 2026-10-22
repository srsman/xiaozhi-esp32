# M5Stack StopWatch (XiaoZhi AI Smart Watch) & MCP Voice Terminal

(English | [中文](README_zh.md) | [日本語](README_ja.md))

This project is a dedicated, fully customized XiaoZhi AI smart firmware built specifically for the **M5Stack StopWatch (AMOLED Round Touchscreen Smart Stopwatch / Smart Watch)**. While fully preserving XiaoZhi AI's realtime large-model conversational voice capabilities (offline wake-up, Muse animated avatar, full-duplex streaming audio, and MCP device control), it deeply integrates M5Stack's 10 native applications tailored for the 466×466 round AMOLED display.

---

## 🌟 Full Feature Matrix

### 1. 10 Deeply Integrated Native Applications
- **XiaoZhi AI Voice Assistant**:
  - Offline low-power voice wake-up ("Nihao XiaoZhi"), speaker recognition;
  - Animated Muse digital avatar displaying rich expressions (listening, thinking, speaking);
  - Low-latency full-duplex realtime LLM voice chat and streaming TTS audio;
  - Device-side MCP (Model Context Protocol) integration for volume, brightness, and peripheral controls.
- **Customizable Smart Badge (Badge)**:
  - **3 Built-in Geek Themes**: XiaoZhi Cyber Badge, Interstellar Pilot Clearance, and Matrix Root Hacker passes;
  - **Full-Screen Touch Gestures**: Swipe left/right across screens, tap left/right display edges to switch, with dynamic bottom dot indicators;
  - **Mobile Wireless Photo Upload**: Long-press any badge to open the confirmation dialog, tap "Edit" to launch an isolated low-power Wi-Fi AP; connect from your smartphone and open `http://192.168.4.1` in any browser to upload, pan/zoom crop, preview, activate, and delete pictures across 6 slots;
  - **Hardware JPEG Decoding & Anti-Black-Screen Fallback**: Accelerated by ESP32-S3 PSRAM and native decoding; gracefully falls back to geek themes if an image is corrupt to prevent black-screen hangs;
  - **Smooth AP Exit**: Tap "Done" on your phone or click the watch side button to cleanly exit AP mode, automatically carousel-focus on the newly active slot, and seamlessly restore background Wi-Fi.
- **Watch Faces**:
  - Classic Analog (smooth sweeping hands), Geek Digital (high-contrast cyber style), and Minimalist Modern;
  - Backed by an RX8130 independent high-precision hardware RTC for power-off timekeeping and automatic NTP sync.
- **Stopwatch**:
  - Millisecond-precision stopwatch engine supporting multi-lap timing and splits.
- **Alarm Clock**:
  - Multi-alarm scheduling with acoustic alerts and tactile flat vibration motor feedback.
- **Lucky Wheel**:
  - Physics-based inertial wheel spinning for fun random choices.
- **Audio Spectrum (Audio FFT)**:
  - Real-time I2S ES8311 codec sampling with high-frame-rate animated column FFT visualization.
- **Spirit Level (IMU)**:
  - Built-in BMI270 6-axis motion sensor providing real-time inclination angles and level bubble guidance.
- **FluidBox**:
  - Gyroscope-driven micro-particle collision simulation reacting to watch tilting.
- **Settings**:
  - Stepless AMOLED brightness slider, volume controls, Wi-Fi status, and system info.
- **3D Physics-Wheel App Launcher**:
  - Innovative 3D physical-wheel roller menu with dynamic text-to-ball alignment and zero-clipping visibility.

---

## 🛠️ Hardware Specifications

| Item | Specification | Description |
| :--- | :--- | :--- |
| **SoC** | ESP32-S3 (Xtensa Dual-Core @ 240MHz) | Hardware vector acceleration support |
| **Memory** | 16MiB Flash + 8MiB Octal PSRAM | High-speed 8-line PSRAM for image and audio buffers |
| **Display** | 1.75" AMOLED Round Touchscreen | 466 × 466 resolution, QSPI interface, CO5300 controller |
| **Touch** | CST820B Capacitive Touch Panel | I2C @ 0x15, supports gestures, swipes, and long presses |
| **Audio Codec** | ES8311 High-Fidelity Audio Codec | I2S full-duplex with hardware amplifier enable control |
| **Power Management** | M5PM1 PMIC | I2C @ 0x6E with precision fuel gauge and battery monitoring |
| **IO Expander** | M5IOE1 IO Expander | I2C @ 0x4F managing display power, resets, and vibration motor |
| **Sensors & RTC** | BMI270 (6-axis IMU) + RX8130 (Hardware RTC) | Motion tracking, attitude computation, and offline timekeeping |
| **Haptic Feedback** | Micro linear flat vibration motor | Tactile touch feedback and alarm vibration alerts |

---

## 🗂️ 16MB Flash Partition Layout

Optimized using `partitions/v2/16m_stopwatch.csv`:

| Partition | Offset | Size | Purpose |
| :--- | :--- | :--- | :--- |
| `nvs` | 0x9000 | 16 KB | Wi-Fi credentials and device preferences |
| `otadata` | 0xd000 | 8 KB | OTA dual-bank switching metadata |
| `ota_0` | 0x20000 | **5.125 MB** | Main running firmware (XiaoZhi AI + 10 native applications) |
| `ota_1` | 0x540000 | **5.125 MB** | Symmetric OTA backup upgrade partition |
| `storage` | 0xa60000 | **1.625 MB** | FATFS filesystem for uploaded user badges |
| `assets` | 0xc00000 | **4.000 MB** | SPIFFS assets (Muse avatars, icons, fonts, sound effects) |

---

## ⚡ Quick Download & Flashing

### 1. Download Firmware
- **Release Package (ZIP)**: Obtain `releases/v2.5.3_m5stack-stopwatch.zip` from this repository or GitHub Actions Artifacts.
- **Merged Binary (BIN)**: Extract `merged-binary.bin` which contains all partitions in one single file.

### 2. Flashing
Connect the M5Stack StopWatch to your computer via USB Type-C (e.g., `COM8`):

```bash
# Fast one-command flash using esptool from offset 0x0
python -m esptool --chip esp32s3 -p COM8 -b 921600 write_flash 0x0 merged-binary.bin
```

Or build and flash via ESP-IDF:
```bash
idf.py -p COM8 flash monitor
```

---

## 📖 Wireless Badge Photo Upload Workflow

1. **Open Badge**: Tap the "Badge" icon in the 3D physics-wheel App Launcher;
2. **Switch Badges**: **Swipe left or right**, or **tap the left/right screen edges** to cycle through built-in geek badges and user photos;
3. **Trigger Edit**: **Long-press** anywhere on the badge screen until the `Enter badge edit?` prompt appears;
4. **Start Hotspot**: Tap the green `[Edit]` button; the screen will display the Wi-Fi AP name (e.g., `M5StopWatch-XXXX`) and IP `http://192.168.4.1`;
5. **Upload from Phone**:
   - Connect your phone to the hotspot (no password required);
   - Open `http://192.168.4.1` in your mobile browser;
   - Select any of the 6 slots, choose a JPG/PNG photo, pan/zoom to crop, and tap "Upload";
   - Tap "Set Active" on the desired slot;
6. **Smooth Exit**:
   - Tap the **[Done]** button at the bottom of the webpage, or click the watch's physical side button;
   - The watch will display `Exited edit mode`, **cleanly return to the badge viewer**, and immediately display your newly uploaded photo.

---

## 🌐 XiaoZhi Ecosystem Documentation

- [M5Stack StopWatch Specific Guide](main/boards/m5stack/stopwatch/README.md)
- [Custom Board Guide](docs/custom-board.md)
- [MCP Protocol Usage](docs/mcp-usage.md)
- [MQTT + UDP Hybrid Protocol](docs/mqtt-udp.md)
- [WebSocket Protocol](docs/websocket.md)

---

## 📄 License

This project is licensed under the MIT License - feel free to use, modify, and build upon it!
