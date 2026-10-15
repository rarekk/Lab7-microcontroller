# 📘 รายงานผลการทำ WokwiLAB — ESP32 + ESP-IDF v6.1 + Wokwi

> สรุปว่าทำอะไรไปบ้าง ได้อะไร ติดอะไร แล้วแก้ยังไง
> วันที่: 15 ตุลาคม 2026

---

## 🧾 ภาพรวม

สร้างโปรเจกต์ ESP-IDF ครบ **4 โมดูล** จากคู่มือ `wokwi.html` ให้ build ผ่านและรันบน Wokwi ได้

| Module | หัวข้อ | สถานะ Build | ขนาด app สุดท้าย |
|:---:|---|:---:|---|
| 1 | Wi-Fi Connectivity (STA) | ✅ ผ่าน | `0xbc570` |
| 2 | I2C Master + LCD1602 | ✅ ผ่าน | `0x28f30` |
| 3 | ISR + FreeRTOS Queue | ✅ ผ่าน | `0x250b0` |
| 4 | Flash File System (SPIFFS) | ✅ ผ่าน | `0x2a1c0` |

**สภาพแวดล้อมที่ใช้**

| รายการ | ค่า |
|---|---|
| Target | ESP32 (DevKitC V4) |
| Framework | ESP-IDF **v6.1** + FreeRTOS |
| ตำแหน่ง IDF | `C:\esp\v6.1\esp-idf` |
| สคริปต์ activate | `C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1` |
| Simulator | Wokwi for VS Code |
| โปรเจกต์ | `C:\microcontrollerPJ\Lab7\module1..4` |

**โครงสร้างไฟล์ต่อโมดูล**

```
moduleN/
├── CMakeLists.txt          # project(moduleN)
├── wokwi.toml              # ชี้ไป build/flasher_args.json
├── diagram.json            # วงจร + board
├── .gitignore
├── (sdkconfig.defaults)    # เฉพาะ module4
├── (partitions.csv)        # เฉพาะ module4
└── main/
    ├── CMakeLists.txt      # REQUIRES ...
    └── main.c
```

---

##  ปัญหาระดับสภาพแวดล้อม (เจอก่อนทุกโมดูล)

ปัญหาเหล่านี้ไม่ได้อยู่ในโค้ด แต่ทำให้ build ไม่ได้เลย

### 1. PowerShell ไม่ยอมรันสคริปต์ activate ของ ESP-IDF

**อาการ**
```
cannot be loaded because running scripts is disabled on this system
idf.py : The term 'idf.py' is not recognized...
```

**สาเหตุ:** Windows Execution Policy ตั้งเป็น `Restricted`

**แก้**
```powershell
# ชั่วคราว เฉพาะหน้าต่างนี้
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# หรือตั้งถาวร (แนะนำ ทำครั้งเดียว)
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

### 2. คำเตือน Unicode ของ IDF

**อาการ**
```
Your environment is not configured to handle Unicode characters.
```

**แก้:** ตั้ง environment variable
```
PYTHONUTF8 = 1
```

### 3. Wokwi ฟ้อง `firmware binary build/flasher_args.json not found`

**สาเหตุ:** ยังไม่เคย build (ยังไม่มีโฟลเดอร์ `build/`) — Wokwi ไม่ compile ให้ ต้อง build เองก่อน

**แก้:** `idf.py set-target esp32` แล้ว `idf.py build` ให้ครบทุกโมดูล

>  ทางที่ง่ายสุดคือใช้ **VS Code → ESP-IDF Terminal** ซึ่ง activate environment ให้อัตโนมัติ

---

## 🟦 Module 1 — Wi-Fi Connectivity (STA)

**เป้าหมาย:** ต่อ Wi-Fi เข้า AP จำลอง `Wokwi-GUEST` และรอ IP ให้ได้

**ทำอะไรไป**
- เขียน `main.c` ตามสเต็ป: `nvs_flash_init()` → `esp_netif_init()` → event loop → สร้าง STA → ลงทะเบียน callback → `esp_wifi_start()` → รอ `xEventGroupWaitBits()`
- ตั้ง `REQUIRES` ของ `main` component

**ได้อะไร**
- เข้าใจว่าทำไม **ต้อง init NVS ก่อน Wi-Fi** (Wi-Fi PHY อ่านค่า RF calibration จาก NVS)
- เข้าใจ event-driven pattern ของ `esp_netif` + default event loop
- ได้ใช้ FreeRTOS **Event Group** เป็นตัว sync ระหว่าง callback กับ `app_main`
- รู้ว่าใน IDF v5+ ตัว `main` **ไม่ auto-require** component พวก `esp_wifi` อีกแล้ว ต้องประกาศเอง

```cmake
REQUIRES nvs_flash esp_event esp_netif esp_wifi
```

**ติดอะไร / แก้ยังไง**

| อาการ | สาเหตุ | แก้ |
|---|---|---|
| `Board not found board-esp32-devkitc-v4` | พิมพ์ชื่อบอร์ดขาด hyphen | ใช้ **`board-esp32-devkit-c-v4`** (มีขีดคั่น) |
| Wokwi หา firmware ไม่เจอ | ใช้ `.elf` + `firmware_signature` ไม่ตรงสเปกใหม่ | เปลี่ยนเป็น `firmware = 'build/flasher_args.json'` |

> 📌 ชื่อบอร์ดที่ถูกต้องเช็คได้จากเอกสาร Wokwi → Diagram Reference
> `board-esp32-devkit-c-v4` = ESP32 official devkit

---

## 🟩 Module 2 — I2C Master + LCD1602

**เป้าหมาย:** คุมจอ LCD1602 ผ่าน I2C (PCF8574 backpack) ที่ address `0x27`

**ทำอะไรไป**
- ใช้ **I2C master driver ตัวใหม่** (`driver/i2c_master.h`) สร้าง bus บน GPIO21 (SDA) / GPIO22 (SCL)
- **เขียน driver ให้จอจริงๆ** เพราะโค้ดเดิมจากคู่มือจอไม่ขึ้น

**ได้อะไร**
- เข้าใจ API ใหม่ของ IDF: `i2c_master_bus_config_t` → `i2c_new_master_bus()` → `i2c_device_config_t` → `i2c_master_bus_add_device()` → `i2c_master_transmit()`
- เข้าใจสถาปัตยกรรม **bus handle vs device handle** (รองรับหลาย slave บนบัสเดียว, thread-safe)
- ได้เขียนโปรโตคอล **HD44780 แบบ 4-bit ผ่าน PCF8574** เอง

**การ map บิต (สำคัญมาก)**

| PCF8574T | HD44780 |
|:---:|:---:|
| P0 | RS |
| P1 | R/W |
| P2 | E |
| P3 | Backlight |
| P4–P7 | D4–D7 |

**ติดอะไร / แก้ยังไง**

| อาการ | สาเหตุ | แก้ |
|---|---|---|
| **จอว่างเปล่า ไม่มีอะไรขึ้น** | โค้ดในคู่มือส่งแค่ byte `0x08` (เปิด backlight) เป็นการ "probe" เท่านั้น — **ไม่มี init HD44780 และไม่มีคำสั่งเขียนข้อความ** | เขียน driver ใหม่: pulse ขา E + ส่งข้อมูล 4-bit + `lcd_init()` + `lcd_print()` |
| ชื่อ pin ใน `diagram.json` | ใช้ `esp:P21`/`esp:P22` ซึ่งบอร์ดนี้ไม่มี | แก้เป็น `esp:21` / `esp:22` (ไม่มี P) |

```c
// หัวใจของการคุยกับจอ: ดีดขา E ให้ HD44780 แลตช์ข้อมูล
uint8_t buf[2] = { data | LCD_EN_BIT, data & ~LCD_EN_BIT };
i2c_master_transmit(s_lcd, buf, sizeof(buf), pdMS_TO_TICKS(1000));
```

**ผลลัพธ์ที่ได้**
```
บรรทัด 1: ESP-IDF v6.1
บรรทัด 2: Uptime: 1s   ← นับวินาทีเพิ่มขึ้นเรื่อยๆ
```

---

## 🟨 Module 3 — ISR + FreeRTOS Queue

**เป้าหมาย:** กดปุ่ม 1 ครั้ง → นับ 1 (ใช้ interrupt + queue ไม่ใช้ polling)

**ทำอะไรไป**
- `GPIO_INTR_NEGEDGE` บน GPIO4 + internal pull-up
- ISR ติด `IRAM_ATTR` และ**สั้นที่สุด**: ส่งแค่เลข pin เข้า queue ด้วย `xQueueSendFromISR()`
- worker task (`xQueueReceive(portMAX_DELAY)`) เป็นคนประมวลผล
- **เพิ่ม debounce** เพราะนับไม่ตรง

**ได้อะไร**
- เข้าใจหลักการ **deferred interrupt processing**: ISR ห้ามทำของหนัก/blocking (ห้าม `vTaskDelay`, `printf`)
- รู้ว่าฟังก์ชันที่เรียกใน ISR ต้องเป็นเวอร์ชัน `...FromISR`
- เข้าใจ **contact bounce** ของปุ่มกดจริง และวิธี debounce ด้วย time window
- แก้ format ให้ถูกต้องกับ `uint32_t`: ใช้ `PRIu32` จาก `<inttypes.h>` (ไม่ใช่ `%ld`)
- cast pointer ผ่าน `uintptr_t` เพื่อกัน warning compiler ใหม่ (GCC 15)

**ติดอะไร / แก้ยังไง**

| อาการ | สาเหตุ | แก้ |
|---|---|---|
| **กด 1 ครั้ง แต่นับหลายครั้ง** | Wokwi **จำลอง button bounce เป็นค่า default** — กดครั้งเดียวเกิด falling edge 10–100 ครั้งใน ~1 ms | เพิ่ม **software debounce 50 ms** ใน worker task |

```c
#define DEBOUNCE_TIME_US 50000   // 50 ms
...
int64_t now_us = esp_timer_get_time();
if (now_us - last_press_us < DEBOUNCE_TIME_US) continue;  // ทิ้งสัญญาณสั่น
last_press_us = now_us;
press_count++;
```

> 💡 มีอีกทางคือใส่ `"bounce": "0"` ใน `diagram.json` แต่ผมไม่ใช้
> เพราะการ debounce ในโค้ด **ถูกต้องกับฮาร์ดแวร์จริงด้วย** ไม่ใช่แค่ปิดปัญหาใน simulator

---

## 🟪 Module 4 — Flash File System (SPIFFS / VFS)

**เป้าหมาย:** mount SPIFFS แล้วเขียน/อ่านไฟล์ด้วย POSIX (`fopen`/`fprintf`/`fgets`) ได้

**ทำอะไรไป**
- สร้าง `partitions.csv` กำหนด partition `storage` เป็น `spiffs` ขนาด 1M
- เปิด custom partition table ผ่าน `sdkconfig.defaults`
- เขียน/อ่านไฟล์ `/spiffs/log.txt` + ตรวจสอบข้อมูลที่อ่านกลับ
- เพิ่ม `esp_spiffs_info()` โชว์พื้นที่ใช้งาน

**ได้อะไร**
- เข้าใจ **partition table** และการกำหนดพื้นที่เก็บข้อมูลเองใน flash
- เข้าใจ **VFS layer** ที่ทำให้เรียก POSIX มาตรฐานได้ เหมือนเขียนไฟล์บน PC
- รู้ว่า partition table ของ custom ต้องมาพร้อม **full image** (bootloader + partition table)

```ini
# sdkconfig.defaults
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
```

**ติดอะไร / แก้ยังไง**

| อาการ | สาเหตุ | แก้ |
|---|---|---|
| `Partitions tables occupies 2.1MB ... does not fit in configured flash size 2MB` | partition รวมกัน 2.1MB แต่ IDF v6.1 default flash = 2MB | ตั้ง flash เป็น **4MB** (ตรงกับ DevKitC V4 จริง) |
| `fprintf` ในคู่มือถูกตัดขึ้นบรรทัดใหม่กลางสตริง | HTML render เพี้ยน | รวมเป็นบรรทัดเดียว + ใส่ `\n` |
| SPIFFS mount ไม่ได้ / พฤติกรรมต่างจากของจริง | โหลด image แบบ app-only | ใช้ `firmware = 'build/flasher_args.json'` โหลด full image |

---

## 🧠 สรุปบทเรียนที่ได้จากทั้ง Lab

1. **ชื่อ path ห้ามมีภาษาไทย/เว้นวรรค** — ของเราปลอดภัยดี (`C:\microcontrollerPJ\Lab7`)
2. **ต้อง build ก่อนเปิด Wokwi เสมอ** — Wokwi ไม่ compile ให้
3. **v6.1 ต่างจาก v5.x ที่ชื่อ component** — ใช้ `esp_driver_i2c`, `esp_driver_gpio` (ของใหม่แยกเป็น `esp_driver_*`)
4. **`main` component ไม่ auto-require** — ต้องเขียน `REQUIRES` เองทุกครั้ง
5. **อ่าน error ให้ครบ** — หลายครั้งคำตอบอยู่ในข้อความ error ตรงๆ เช่น `does not fit in configured flash size 2MB`
6. **Simulator ≠ ฮาร์ดแวร์จริง** — Wokwi มี bounce ของปุ่ม, ไม่จำลอง contrast ของจอ, และมี default flash 4MB

---

## ✅ Checklist การรันแต่ละโมดูล

```powershell
# 1. เปิด ESP-IDF environment (หรือใช้ ESP-IDF Terminal ใน VS Code)
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1

# 2. เข้าโฟลเดอร์โมดูล แล้ว build
cd C:\microcontrollerPJ\Lab7\module1
idf.py set-target esp32
idf.py build

# 3. เปิด Wokwi
#    F1 → Wokwi: Start Simulator
#    (Workspace ต้องเป็นโฟลเดอร์ของโมดูลนั้น ไม่ใช่ Lab7 ทั้งก้อน)
```

**ก่อนจะกด Start Simulator ทุกครั้ง:**
- [ ] อยู่ที่ Workspace = โฟลเดอร์โมดูลที่ต้องการ
- [ ] `idf.py build` ผ่าน (มี `build/flasher_args.json`)
- [ ] ถ้าแก้โค้ด → build ใหม่ แล้ว Start Simulator ใหม่

---

## 📋 สรุปสิ่งที่แก้จากคู่มือเดิม

| # | ไฟล์ | แก้จาก | เป็น |
|---|---|---|---|
| 1 | ทุก `diagram.json` | `board-esp32-devkitc-v4` | `board-esp32-devkit-c-v4` |
| 2 | `module2/diagram.json` | `esp:P21`, `esp:P22` | `esp:21`, `esp:22` |
| 3 | `module3/diagram.json` | `esp:P4` | `esp:4` |
| 4 | ทุก `wokwi.toml` | `.elf` + `firmware_signature` | `firmware = 'build/flasher_args.json'` |
| 5 | `module2/main/main.c` | probe อย่างเดียว | + LCD driver เต็ม + แสดงข้อความ |
| 6 | `module3/main/main.c` | ไม่มี debounce | + debounce 50 ms |
| 7 | `module4/sdkconfig.defaults` | ไม่มี | + custom partition + flash 4MB |
| 8 | ทุก `main/CMakeLists.txt` | ไม่มี REQUIRES | + REQUIRES ที่ถูกต้องของแต่ละโมดูล |

---

*รายงานนี้สรุปจากเซสชันแก้ไขจริง — ทุกโมดูล build ผ่านด้วย ESP-IDF v6.1* 🎉
