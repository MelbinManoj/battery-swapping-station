// Battery Swapping Station - Arduino Uno
// Three battery bays. Each bay detects whether a battery is inserted, measures
// its voltage, charges it through a relay and has a solenoid door lock.
// Pressing SWAP unlocks an empty bay for the depleted battery, then unlocks a
// bay holding a fully charged battery for the driver to take.
//
// Wiring (per bay i = 0..2):
//   Battery-present switch: D2, D3, D4 -> GND   (INPUT_PULLUP, LOW = battery in)
//   Charger relay IN:       D5, D6, D7          (active LOW)
//   Door lock relay IN:     D8, D9, D10         (active LOW, ON = unlocked)
//   Voltage divider output: A0, A1, A2          (e.g. 30k/7.5k module, max 25 V)
//   SWAP push button: A3 -> GND (INPUT_PULLUP)   Buzzer: D11   Ready LED: D13

#include "station_controller.h"

const uint8_t PRESENT_PIN[NUM_SLOTS] = {2, 3, 4};
const uint8_t CHARGE_PIN[NUM_SLOTS]  = {5, 6, 7};
const uint8_t LOCK_PIN[NUM_SLOTS]    = {8, 9, 10};
const uint8_t VOLT_PIN[NUM_SLOTS]    = {A0, A1, A2};
const uint8_t SWAP_BUTTON = A3;
const uint8_t BUZZER_PIN = 11;
const uint8_t READY_LED = 13;
const float DIVIDER_RATIO = 5.0;   // (30k + 7.5k) / 7.5k

StationConfig config = {
  12.6,          // full voltage (3S Li-ion)
  9.0,           // below 9 V the pack is considered damaged
  14400000UL,    // 4 h maximum charge time
  60000UL        // 60 s for the driver to insert / remove a battery
};

StationController station(config);
SlotInputs inputs[NUM_SLOTS];
SlotOutputs outputs[NUM_SLOTS];
bool lastButton = HIGH;
unsigned long lastPrint = 0;

float readVolts(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (sum / 8.0) * 5.0 / 1023.0 * DIVIDER_RATIO;
}

void relay(uint8_t pin, bool on) { digitalWrite(pin, on ? LOW : HIGH); }  // active LOW modules

const char* slotName(SlotState s) {
  switch (s) {
    case SLOT_EMPTY: return "EMPTY";
    case SLOT_CHARGING: return "CHARGING";
    case SLOT_FULL: return "FULL";
    default: return "FAULT";
  }
}

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < NUM_SLOTS; i++) {
    pinMode(PRESENT_PIN[i], INPUT_PULLUP);
    pinMode(CHARGE_PIN[i], OUTPUT); relay(CHARGE_PIN[i], false);
    pinMode(LOCK_PIN[i], OUTPUT);   relay(LOCK_PIN[i], false);
  }
  pinMode(SWAP_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(READY_LED, OUTPUT);
  Serial.println(F("Battery Swapping Station ready"));
}

void loop() {
  unsigned long now = millis();

  bool button = digitalRead(SWAP_BUTTON);
  if (lastButton == HIGH && button == LOW) {          // button pressed
    if (station.requestSwap(now)) {
      Serial.println(F("Swap started: insert your battery into the open bay"));
      tone(BUZZER_PIN, 2000, 150);
    } else {
      Serial.println(F("Swap not available (no charged battery or no free bay)"));
      tone(BUZZER_PIN, 400, 600);
    }
    delay(30);                                         // debounce
  }
  lastButton = button;

  for (int i = 0; i < NUM_SLOTS; i++) {
    inputs[i].present = digitalRead(PRESENT_PIN[i]) == LOW;
    inputs[i].volts = inputs[i].present ? readVolts(VOLT_PIN[i]) : 0;
  }
  SwapStep before = station.getStep();
  station.update(inputs, outputs, now);
  for (int i = 0; i < NUM_SLOTS; i++) {
    relay(CHARGE_PIN[i], outputs[i].charging);
    relay(LOCK_PIN[i], outputs[i].unlocked);
  }
  if (before == SWAP_AWAIT_RETURN && station.getStep() == SWAP_AWAIT_PICKUP)
    Serial.println(F("Battery received. Take your charged battery from the open bay"));
  if (before == SWAP_AWAIT_PICKUP && station.getStep() == SWAP_IDLE)
    Serial.println(F("Swap finished. Drive safe!"));

  digitalWrite(READY_LED, station.fullCount() > 0 ? HIGH : LOW);

  if (now - lastPrint >= 3000) {
    lastPrint = now;
    for (int i = 0; i < NUM_SLOTS; i++) {
      Serial.print(F("Bay ")); Serial.print(i + 1); Serial.print(F(": "));
      Serial.print(slotName(station.slot(i)));
      Serial.print(F(" (")); Serial.print(inputs[i].volts, 2); Serial.print(F(" V)  "));
    }
    Serial.print(F("swaps=")); Serial.println(station.completedSwaps());
  }
}
