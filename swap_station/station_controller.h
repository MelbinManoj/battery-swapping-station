// Pure control logic for the battery swapping station.
// No Arduino dependencies, so it can be unit-tested on a PC.
#ifndef STATION_CONTROLLER_H
#define STATION_CONTROLLER_H

#include <stdint.h>

#define NUM_SLOTS 3

struct StationConfig {
  float fullVolts;          // stop charging at this voltage (e.g. 12.6 V for a 3S Li-ion pack)
  float minHealthyVolts;    // below this a battery is treated as faulty
  uint32_t maxChargeMs;     // charging longer than this marks the slot faulty
  uint32_t doorTimeoutMs;   // cancel a swap step if nobody acts in time
};

enum SlotState { SLOT_EMPTY, SLOT_CHARGING, SLOT_FULL, SLOT_FAULT };
enum SwapStep { SWAP_IDLE, SWAP_AWAIT_RETURN, SWAP_AWAIT_PICKUP };

struct SlotInputs { bool present; float volts; };
struct SlotOutputs { bool charging; bool unlocked; };

class StationController {
 public:
  explicit StationController(const StationConfig& c)
      : cfg(c), step(SWAP_IDLE), returnSlot(-1), pickupSlot(-1), stepSince(0), swaps(0) {
    for (int i = 0; i < NUM_SLOTS; i++) { slots[i] = SLOT_EMPTY; chargeStart[i] = 0; wasPresent[i] = false; }
  }

  // Customer pressed the SWAP button. Returns false if a swap is impossible right now.
  bool requestSwap(uint32_t nowMs) {
    if (step != SWAP_IDLE) return false;
    int empty = findSlot(SLOT_EMPTY), full = findSlot(SLOT_FULL);
    if (empty < 0 || full < 0) return false;    // need a free bay and a charged battery
    returnSlot = empty; pickupSlot = full;
    setStep(SWAP_AWAIT_RETURN, nowMs);
    return true;
  }

  // Call every loop with the sensor readings; fills in what each slot should do.
  void update(const SlotInputs in[NUM_SLOTS], SlotOutputs out[NUM_SLOTS], uint32_t nowMs) {
    for (int i = 0; i < NUM_SLOTS; i++) {
      bool inserted = in[i].present && !wasPresent[i];
      bool removed = !in[i].present && wasPresent[i];
      wasPresent[i] = in[i].present;

      if (removed) slots[i] = SLOT_EMPTY;
      else if (inserted) startCharging(i, in[i].volts, nowMs);
      else if (slots[i] == SLOT_CHARGING) {
        if (in[i].volts >= cfg.fullVolts) slots[i] = SLOT_FULL;
        else if (nowMs - chargeStart[i] >= cfg.maxChargeMs) slots[i] = SLOT_FAULT;
      }
    }

    // Swap sequence: deposit the depleted battery, then collect a charged one.
    if (step == SWAP_AWAIT_RETURN) {
      if (in[returnSlot].present) setStep(SWAP_AWAIT_PICKUP, nowMs);
      else if (nowMs - stepSince >= cfg.doorTimeoutMs) cancel(nowMs);
    } else if (step == SWAP_AWAIT_PICKUP) {
      if (!in[pickupSlot].present) { swaps++; cancel(nowMs); }
      else if (nowMs - stepSince >= cfg.doorTimeoutMs) cancel(nowMs);
    }

    for (int i = 0; i < NUM_SLOTS; i++) {
      out[i].charging = (slots[i] == SLOT_CHARGING);
      out[i].unlocked = (step == SWAP_AWAIT_RETURN && i == returnSlot) ||
                        (step == SWAP_AWAIT_PICKUP && i == pickupSlot);
    }
  }

  SlotState slot(int i) const { return slots[i]; }
  SwapStep getStep() const { return step; }
  int completedSwaps() const { return swaps; }
  int fullCount() const { int n = 0; for (int i = 0; i < NUM_SLOTS; i++) n += slots[i] == SLOT_FULL; return n; }

 private:
  void startCharging(int i, float volts, uint32_t nowMs) {
    if (volts < cfg.minHealthyVolts) { slots[i] = SLOT_FAULT; return; }
    if (volts >= cfg.fullVolts) { slots[i] = SLOT_FULL; return; }
    slots[i] = SLOT_CHARGING; chargeStart[i] = nowMs;
  }
  int findSlot(SlotState s) const { for (int i = 0; i < NUM_SLOTS; i++) if (slots[i] == s) return i; return -1; }
  void setStep(SwapStep s, uint32_t nowMs) { step = s; stepSince = nowMs; }
  void cancel(uint32_t nowMs) { setStep(SWAP_IDLE, nowMs); returnSlot = pickupSlot = -1; }

  StationConfig cfg;
  SlotState slots[NUM_SLOTS];
  uint32_t chargeStart[NUM_SLOTS];
  bool wasPresent[NUM_SLOTS];
  SwapStep step;
  int returnSlot, pickupSlot;
  uint32_t stepSince;
  int swaps;
};

#endif
