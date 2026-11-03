/*
  Smart Library Management System — Arduino Mega Firmware
  ---------------------------------------------------------
  Hardware:
    - 5x HC-SR04 ultrasonic sensors (shared trigger pin)
        TRIG_PIN   = A1  (common trigger for all 5 sensors)
        ECHO_PINS  = A2, A3, A4, A5, A6   (one echo pin per shelf slot)
    - 5x LEDs (one per shelf slot)
        LED_PINS   = 2, 3, 4, 5, 6
    - 1x Buzzer
        BUZZER_PIN = A7

  Serial protocol (9600 baud, each message ends with '\n'):

    PC -> Arduino
      STATUS             ask for current availability of all 5 slots
      SELECT:<n>         user picked the book in slot n (0-4): light LED n
                          and start watching for it to be removed
      RESET              cancel any active selection, turn off LEDs/buzzer

    Arduino -> PC
      READY                     sent once at startup
      STATUS:1,1,0,1,1          reply to STATUS (1 = book present, 0 = empty)
      TAKEN:<n>                 correct book removed from slot n
      WRONG:<n>                 a book was removed from slot n while a
                                 DIFFERENT slot was the expected one
      RETURNED:<n>               a book was placed back into slot n
*/

const int SENSOR_COUNT = 5;
const int TRIG_PIN = A1;
const int ECHO_PINS[SENSOR_COUNT] = {A2, A3, A4, A5, A6};
const int LED_PINS[SENSOR_COUNT]  = {2, 3, 4, 5, 6};
const int BUZZER_PIN = A7;

// ---- Tunable settings ----
const int PRESENCE_THRESHOLD_CM = 15;          // distance below this = "book present"
                                                // measure your shelf depth and adjust!
const unsigned long ECHO_TIMEOUT_US = 6000UL;  // ~1 m max range, keeps the loop fast
const uint8_t CONFIRM_COUNT = 3;               // consecutive matching reads needed
                                                // before a state change is accepted
const unsigned long SCAN_INTERVAL_MS = 60;     // delay between full 5-sensor scans
const unsigned long BUZZER_DURATION_MS = 3000; // auto-silence if never resolved

// ---- State ----
bool confirmedPresent[SENSOR_COUNT];
bool pendingState[SENSOR_COUNT];
uint8_t pendingCount[SENSOR_COUNT];

bool selectionActive = false;
int  expectedSlot = -1;

bool buzzerActive = false;
unsigned long buzzerStartTime = 0;

unsigned long lastScanTime = 0;
String serialBuffer = "";

void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(ECHO_PINS[i], INPUT);
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  for (int i = 0; i < SENSOR_COUNT; i++) {
    confirmedPresent[i] = isPresent(i);
    pendingState[i] = confirmedPresent[i];
    pendingCount[i] = CONFIRM_COUNT;
  }

  Serial.println("READY");
}

void loop() {
  readSerialCommands();

  unsigned long now = millis();
  if (now - lastScanTime >= SCAN_INTERVAL_MS) {
    lastScanTime = now;
    scanAllSensors();
  }

  if (buzzerActive && (millis() - buzzerStartTime >= BUZZER_DURATION_MS)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

// ---------- Sensor reading ----------

long measureDistanceCM(int sensorIndex) {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PINS[sensorIndex], HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) {
    return -1; // no echo within timeout = nothing in range = treat as "empty"
  }
  return duration / 58; // microseconds -> cm
}

bool isPresent(int sensorIndex) {
  long d = measureDistanceCM(sensorIndex);
  return (d > 0 && d < PRESENCE_THRESHOLD_CM);
}

void scanAllSensors() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    bool reading = isPresent(i);

    if (reading == pendingState[i]) {
      if (pendingCount[i] < 255) pendingCount[i]++;
    } else {
      pendingState[i] = reading;
      pendingCount[i] = 1;
    }

    if (pendingCount[i] >= CONFIRM_COUNT && pendingState[i] != confirmedPresent[i]) {
      confirmedPresent[i] = pendingState[i];
      handleStateChange(i, confirmedPresent[i]);
    }
  }
}

// ---------- Event handling ----------

void handleStateChange(int slot, bool nowPresent) {
  if (!nowPresent) {
    // A book was just removed from `slot`
    if (selectionActive && slot == expectedSlot) {
      digitalWrite(LED_PINS[slot], LOW);
      selectionActive = false;
      expectedSlot = -1;
      Serial.print("TAKEN:");
      Serial.println(slot);
    } else {
      if (selectionActive) {
        activateBuzzer();
      }
      Serial.print("WRONG:");
      Serial.println(slot);
    }
  } else {
    Serial.print("RETURNED:");
    Serial.println(slot);

    if (buzzerActive) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzerActive = false;
    }
  }
}

void activateBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerActive = true;
  buzzerStartTime = millis();
}

// ---------- Serial command handling ----------

void readSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      processCommand(serialBuffer);
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "STATUS") {
    sendStatus();
  } else if (cmd.startsWith("SELECT:")) {
    int slot = cmd.substring(7).toInt();
    if (slot >= 0 && slot < SENSOR_COUNT) {
      startSelection(slot);
    }
  } else if (cmd == "RESET") {
    resetSelection();
  }
}

void sendStatus() {
  Serial.print("STATUS:");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(confirmedPresent[i] ? "1" : "0");
    if (i < SENSOR_COUNT - 1) Serial.print(",");
  }
  Serial.println();
}

void startSelection(int slot) {
  for (int i = 0; i < SENSOR_COUNT; i++) digitalWrite(LED_PINS[i], LOW);
  digitalWrite(BUZZER_PIN, LOW);
  buzzerActive = false;

  expectedSlot = slot;
  selectionActive = true;
  digitalWrite(LED_PINS[slot], HIGH);
}

void resetSelection() {
  selectionActive = false;
  expectedSlot = -1;
  for (int i = 0; i < SENSOR_COUNT; i++) digitalWrite(LED_PINS[i], LOW);
  digitalWrite(BUZZER_PIN, LOW);
  buzzerActive = false;
}
