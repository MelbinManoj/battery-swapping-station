# 🔋 Battery Swapping Station

> An automated EV battery swapping station prototype: drop off a depleted battery, take a fully charged one, with charging and door locks managed by an Arduino.

![badge](https://img.shields.io/badge/Arduino%20Uno-00878F?logo=arduino&logoColor=white&style=flat) ![badge](https://img.shields.io/badge/C%2B%2B-00599C?logo=cplusplus&logoColor=white&style=flat) ![badge](https://img.shields.io/badge/Power%20Electronics-E34F26?style=flat)
![tests](https://github.com/MelbinManoj/battery-swapping-station/actions/workflows/tests.yml/badge.svg)

> 📝 **About this code:** I built this project during my B.Tech, but the original source code was lost. The code here is a **rebuilt version (2026)** based on the original design. It compiles for the Arduino Uno, and its logic is checked by automated tests that run on simulated sensor data. It has not yet been re-tested on the physical hardware.

## 📖 Overview

Long charging times hold back electric vehicles. A swapping station **exchanges a depleted battery for a charged one in minutes**. This prototype manages three battery bays. Each bay detects when a battery is inserted, measures its voltage, charges it through a relay and has a solenoid door lock. The station runs the whole swap step by step.

## ✨ Features

- 3 bays, each with presence detection, voltage measurement, a charger relay and a door lock
- Per-bay states: `EMPTY`, `CHARGING`, `FULL` and `FAULT`
- Stops charging at **12.6 V** (3S Li-ion) and flags packs **below 9 V** as damaged
- **Overcharge protection**: charging that runs longer than 4 h marks the bay as faulty
- Guided swap: the **empty bay opens first** for the return, then the **full bay opens** for pickup
- Refuses a swap when no charged battery is available, and relocks doors if nobody acts within 60 s

**States:** Swap: `IDLE` → press SWAP → `AWAIT_RETURN` (empty bay unlocked) → battery inserted → `AWAIT_PICKUP` (full bay unlocked) → battery taken → `IDLE`

## 🧰 Components

| Component | Qty |
|---|:---:|
| Arduino Uno | 1 |
| Limit / micro switches (battery present) | 3 |
| Voltage sensor modules (0–25 V divider) | 3 |
| Relay modules: chargers + door locks | 6 |
| 12 V solenoid locks | 3 |
| 3S Li-ion chargers / CC-CV modules | 3 |
| Push button, buzzer, LED | 1 each |

## 🔌 Wiring

| Connection | Arduino pin |
|---|---|
| Battery-present switches | D2, D3, D4 → GND (INPUT_PULLUP) |
| Charger relays | D5, D6, D7 (active LOW) |
| Door-lock relays | D8, D9, D10 (active LOW, ON = unlocked) |
| Voltage sensors | A0, A1, A2 |
| SWAP button | A3 → GND |
| Buzzer / Ready LED | D11 / D13 |

## 🚀 Getting Started

1. Open [`swap_station/swap_station.ino`](./swap_station/swap_station.ino) in the Arduino IDE. The `station_controller.h` file in the same folder opens with it.
2. Select **Arduino Uno** (or Nano) and upload. No extra libraries are needed.
3. Open the Serial Monitor to watch the live readings.
4. **Calibrate:** Check `DIVIDER_RATIO` against a multimeter reading, and set `fullVolts` / `minHealthyVolts` for your battery chemistry.

## 🧪 Tests

All decision logic lives in [`station_controller.h`](./swap_station/station_controller.h), with no Arduino dependencies, so it can be tested on a PC with simulated sensor data:

```bash
cd test
g++ -std=c++11 -I../swap_station test_station_controller.cpp -o test && ./test
```

What's tested:

- ✅ Charging stops at 12.6 V
- ✅ Damaged packs are flagged and never charged
- ✅ Charging timeout marks the bay faulty
- ✅ Swap is refused without a charged battery
- ✅ Full swap sequence: correct doors unlock in order, the returned battery starts charging and the swap is counted
- ✅ Doors relock after a 60 s timeout

These tests run automatically on every push with GitHub Actions.

## 📂 Project Structure

```text
battery-swapping-station/
├── swap_station/
│   ├── swap_station.ino      # hardware layer: pins, sensors, outputs
│   └── station_controller.h  # decision logic (unit-tested)
├── test/
│   └── test_station_controller.cpp
└── README.md
```

## 👤 Author

**Melbin Manoj** — [GitHub](https://github.com/MelbinManoj) · [LinkedIn](https://www.linkedin.com/in/melbin-manoj-8ba512269/)
