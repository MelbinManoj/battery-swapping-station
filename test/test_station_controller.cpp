// Host-side unit tests for the battery swapping station logic.
// Build & run:  g++ -std=c++11 -I../swap_station test_station_controller.cpp -o t && ./t
#include <cstdio>
#include "station_controller.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static StationConfig cfg() { return {12.6f, 9.0f, 14400000UL, 60000UL}; }

struct Sim {
  StationController st; SlotInputs in[NUM_SLOTS]; SlotOutputs out[NUM_SLOTS]; uint32_t t;
  Sim() : st(cfg()), t(0) { for (int i = 0; i < NUM_SLOTS; i++) in[i] = {false, 0}; }
  void tick(uint32_t ms = 100) { t += ms; st.update(in, out, t); }
};

int main() {
  // insert a depleted battery -> charging relay on; reaches 12.6 V -> FULL, relay off
  { Sim s; s.in[0] = {true, 10.8f}; s.tick();
    CHECK(s.st.slot(0) == SLOT_CHARGING); CHECK(s.out[0].charging);
    s.in[0].volts = 12.0f; s.tick(60000); CHECK(s.st.slot(0) == SLOT_CHARGING);
    s.in[0].volts = 12.6f; s.tick(); CHECK(s.st.slot(0) == SLOT_FULL); CHECK(!s.out[0].charging); }

  // damaged pack (too low) is flagged and never charged
  { Sim s; s.in[1] = {true, 6.0f}; s.tick();
    CHECK(s.st.slot(1) == SLOT_FAULT); CHECK(!s.out[1].charging); }

  // charging that never finishes times out to FAULT (overcharge protection)
  { Sim s; s.in[2] = {true, 11.0f}; s.tick();
    s.tick(14400000UL); CHECK(s.st.slot(2) == SLOT_FAULT); CHECK(!s.out[2].charging); }

  // swap is refused when there is no charged battery
  { Sim s; s.in[0] = {true, 11.0f}; s.tick();
    CHECK(!s.st.requestSwap(s.t)); }

  // full swap: bay 0 full, bays 1-2 empty
  { Sim s; s.in[0] = {true, 12.7f}; s.tick(); CHECK(s.st.slot(0) == SLOT_FULL);
    CHECK(s.st.requestSwap(s.t)); s.tick();
    CHECK(s.st.getStep() == SWAP_AWAIT_RETURN);
    CHECK(s.out[1].unlocked); CHECK(!s.out[0].unlocked);     // empty bay opens first
    CHECK(!s.st.requestSwap(s.t));                           // no double swaps
    s.in[1] = {true, 10.5f}; s.tick();                       // driver returns depleted battery
    CHECK(s.st.getStep() == SWAP_AWAIT_PICKUP);
    CHECK(!s.out[1].unlocked); CHECK(s.out[0].unlocked);     // returned bay locks, full bay opens
    CHECK(s.st.slot(1) == SLOT_CHARGING);                    // starts charging right away
    s.in[0] = {false, 0}; s.tick();                          // driver takes the charged battery
    CHECK(s.st.getStep() == SWAP_IDLE); CHECK(s.st.completedSwaps() == 1);
    CHECK(s.st.slot(0) == SLOT_EMPTY); CHECK(!s.out[0].unlocked); }

  // nobody inserts a battery -> door relocks after the timeout
  { Sim s; s.in[0] = {true, 12.7f}; s.tick();
    CHECK(s.st.requestSwap(s.t)); s.tick();
    s.tick(60000); CHECK(s.st.getStep() == SWAP_IDLE);
    for (int i = 0; i < NUM_SLOTS; i++) CHECK(!s.out[i].unlocked);
    CHECK(s.st.completedSwaps() == 0); }

  if (failures == 0) printf("All station controller tests passed.\n");
  return failures == 0 ? 0 : 1;
}
