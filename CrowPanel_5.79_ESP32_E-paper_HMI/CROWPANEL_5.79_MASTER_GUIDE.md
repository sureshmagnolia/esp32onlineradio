# Elecrow CrowPanel 5.79" ESP32-S3 E-Paper HMI Display (272×792) — Complete Technical Handbook

**Product Model**: CrowPanel ESP32 5.79" E-Paper HMI Display with 272×792 Resolution  
**Manufacturer**: [Elecrow](https://www.elecrow.com/)  
**GitHub Repository**: [Elecrow-RD/CrowPanel-ESP32-5.79-E-paper-HMI-Display-with-272-792](https://github.com/Elecrow-RD/CrowPanel-ESP32-5.79-E-paper-HMI-Display-with-272-792)  
**Local Workspace Folder**: `d:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI`

---

## 1. Executive Hardware Summary

| Subsystem | Specification |
| :--- | :--- |
| **Main SoC** | Espressif **ESP32-S3-WROOM-1-N8R8** (Dual-core Xtensa LX7 @ 240 MHz) |
| **Internal RAM** | 512 KB SRAM |
| **PSRAM** | **8 MB Octal PSRAM** (OPI mode, 80 MHz) |
| **Flash Memory** | **8 MB SPI Flash** (Quad/Octal SPI) |
| **Display Panel** | 5.79-inch Active Matrix Electrophoretic Display (AM EPD) |
| **Display Resolution** | **272 × 792 pixels** (ultra-wide / tall aspect ratio) |
| **Pixel Pitch** | 0.1755 × 0.1755 mm |
| **Active Area** | 47.74 mm (H) × 139.00 mm (L) |
| **Display Controller** | **Dual SSD1683** (`SSD1683*2` driving 272×792 matrix) |
| **Display Color** | 1-bit Monochrome (Black & White) |
| **Refresh Modes** | Full / Global refresh & Fast Partial refresh |
| **Physical Controls** | 5 On-board buttons (Home, Exit, Dial Left/Prev, Dial Right/Next, Dial Press/OK) + Reset + Boot |
| **Expansion Storage** | MicroSD / TF Card Slot (Dedicated SPI bus) |
| **Power Indicators** | Power LED (GPIO 41) + Onboard Charge management |
| **Operating Voltage** | 5.0 V DC (USB Type-C) / 3.7 V Lithium Battery connector (BAT) |

---

## 2. Complete GPIO Pin Assignment Table

### A. E-Paper Display Interface (Dual SSD1683 SPI)
| Pin Name | ESP32-S3 GPIO | Function / Notes |
| :--- | :---: | :--- |
| **EPD_PWR** | `GPIO 7` | **Display Power Enable** (Drive **HIGH** in setup to power the E-Paper panel) |
| **EPD_SCK** | `GPIO 12` | SPI Clock |
| **EPD_MOSI** | `GPIO 11` | SPI Master Out Slave In |
| **EPD_RES** | `GPIO 47` | Hardware Reset line |
| **EPD_DC** | `GPIO 46` | Data / Command Selection line |
| **EPD_CS** | `GPIO 45` | SPI Chip Select (Active LOW) |
| **EPD_BUSY** | `GPIO 48` | Display Busy Status line (Input) |

> [!IMPORTANT]
> **CRITICAL SCREEN POWER CONTROL**:  
> In your `setup()` function, you **MUST** drive `GPIO 7` to **HIGH** to enable the power converter for the E-Paper panel:
> ```cpp
> pinMode(7, OUTPUT);
> digitalWrite(7, HIGH);
> ```

---

### B. MicroSD / TF Card Interface (HSPI Bus)
| Pin Name | ESP32-S3 GPIO | Function / Notes |
| :--- | :---: | :--- |
| **TF_PWR** | `GPIO 42` | **TF / Peripheral Power Enable** (Drive **HIGH** in setup) |
| **TF_CS** | `GPIO 10` | MicroSD SPI Chip Select |
| **TF_SCK** | `GPIO 39` | MicroSD SPI Clock |
| **TF_MOSI** | `GPIO 40` | MicroSD SPI MOSI |
| **TF_MISO** | `GPIO 13` | MicroSD SPI MISO |

```cpp
pinMode(42, OUTPUT);
digitalWrite(42, HIGH);
SPIClass SD_SPI(HSPI);
SD_SPI.begin(39, 13, 40); // SCK=39, MISO=13, MOSI=40
SD.begin(10, SD_SPI, 80000000); // CS=10
```

---

### C. Physical Navigation Buttons & Dial Switch
| Button Name | ESP32-S3 GPIO | Active State / Usage |
| :--- | :---: | :--- |
| **EXIT_KEY** | `GPIO 1` | Back / Exit button (Active LOW) |
| **HOME_KEY** | `GPIO 2` | Home button (Active LOW) |
| **NEXT_KEY** | `GPIO 4` | Dial Switch Down / Next (Active LOW) |
| **OK_KEY** | `GPIO 5` | Dial Switch Push / Confirm (Active LOW) |
| **PRV_KEY** | `GPIO 6` | Dial Switch Up / Previous (Active LOW) |
| **POWER_LED**| `GPIO 41` | Green Power / Status LED (Active HIGH) |
| **BOOT** | `GPIO 0` | ESP32 Bootloader / User Button |

---

### D. Expansion Header GPIOs
| Header Pins | ESP32-S3 GPIOs |
| :--- | :--- |
| **Available I/O** | `GPIO 8`, `GPIO 3`, `GPIO 14`, `GPIO 9`, `GPIO 16`, `GPIO 15`, `GPIO 18`, `GPIO 17`, `GPIO 20`, `GPIO 19`, `GPIO 38`, `GPIO 21` |
| **UART0** | `TX = GPIO 43`, `RX = GPIO 44` (UART connector) |

---

## 3. Stock Factory Firmware Flashing & Restoration

The precompiled factory firmware binaries are preserved locally at:  
`d:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI\factory_firmware\Epaper-5.79(E) Inch\`

### Partition & Flash Offsets:
| Binary File | Flash Address | Description |
| :--- | :---: | :--- |
| `main.ino.bootloader.bin` | **`0x0000`** | ESP32-S3 Stage 2 Bootloader |
| `main.ino.partitions.bin` | **`0x8000`** | Partition Table |
| `boot_app0.bin` | **`0xe000`** | OTA Bootloader Descriptor / App State |
| `main.ino.bin` | **`0x10000`** | Factory Application Firmware (~1.8 MB) |

### One-Click Restoration via `esptool.py`:
Run the following command from PowerShell/Command Prompt (substituting `COMx` with your board's serial port):

```powershell
python -m esptool --chip esp32s3 --port COMx --baud 921600 write_flash `
  0x0000 "d:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI\factory_firmware\Epaper-5.79(E) Inch\main.ino.bootloader.bin" `
  0x8000 "d:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI\factory_firmware\Epaper-5.79(E) Inch\main.ino.partitions.bin" `
  0xe000 "d:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI\factory_firmware\Epaper-5.79(E) Inch\boot_app0.bin" `
  0x10000 "d:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI\factory_firmware\Epaper-5.79(E) Inch\main.ino.bin"
```

---

## 4. Arduino IDE Recommended Board Configuration

* **Board**: `ESP32S3 Dev Module`
* **Port**: Select your COM Port
* **USB CDC On Boot**: `Enabled` (for Serial debug monitor over native USB-C)
* **CPU Frequency**: `240MHz (WiFi)`
* **Flash Size**: `8MB (64Mb)`
* **Flash Mode**: `QIO 80MHz`
* **PSRAM**: `OPI PSRAM`
* **Partition Scheme**: `8M with spiffs` / `8M with fat`

---

## 5. Local Repository Structure

```
d:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI\
├── 3D file\                                   # 3D STEP mechanical model of the module
├── Datasheet\
│   ├── SSD1683_Datasheet.pdf                  # Display driver IC datasheet
│   └── esp32-s3-wroom-1_datasheet.pdf         # ESP32-S3 module datasheet
├── Eagle_SCH&PCB\
│   └── CrowPanel-ESP32-Display-5.79E-Inch\
│       ├── CrowPanel ESP32 Display-5.79(E) Inch.pdf  # Full schematic diagram
│       ├── CrowPanel ESP32 Display-5.79(E) Inch.sch  # Eagle schematic
│       └── CrowPanel ESP32 Display-5.79(E) Inch.brd  # Eagle PCB layout
├── example\
│   └── arduino\
│       ├── Demos\                             # OpenWeather & WiFi/BLE refresh demos
│       ├── Examples\                          # Standalone demos for Buttons, EPD refresh, TF, Power, GPIO
│       └── libraries\                         # Bundled EPD, GxEPD2, Adafruit_GFX, ArduinoJson, etc.
├── factory_firmware\                          # Factory binary files ready for flashing
├── factory_sourecode\                         # Full C++/Arduino source code of factory demo
└── CROWPANEL_5.79_MASTER_GUIDE.md             # This comprehensive technical guide
```
