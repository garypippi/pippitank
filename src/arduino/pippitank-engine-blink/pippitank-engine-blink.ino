// Phase A: PWR_EN drive test (white-bulb tactic)
//   PB0 = PWR_EN -> Q2 Gate -> Q1 (Pch) -> post-MOSFET rail
//   HIGH = ON, LOW = OFF
//
// Pre-conditions:
//   - SW1 fixed at pos1 (rev A Q2 source workaround)
//   - BAT+ from current-limited bench supply (NOT LiPo for first run)
//   - LED + 1k ohm tied between post-MOSFET node and GND as witness load
//   - VCC (ATtiny) = 5V from Arduino Uno (jtag2updi host)

void setup() {
  pinMode(PIN_PB0, OUTPUT);
  digitalWrite(PIN_PB0, LOW);
  delay(5000);
}

void loop() {
  digitalWrite(PIN_PB0, HIGH);
  delay(2000);
  digitalWrite(PIN_PB0, LOW);
  delay(2000);
}
