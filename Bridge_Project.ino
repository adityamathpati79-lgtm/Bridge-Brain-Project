/*
  Bridge_Project.ino
  HX711 DT = 4, SCK = 5
  IR Entry = 2 (goes LOW when object detected)
  IR Exit  = 3 (goes LOW when object detected)
  Calibration factor = 400.0
  Outputs one JSON object line per event to Serial.
*/

#include "HX711.h"

const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN  = 5;
const int IR_ENTRY_PIN      = 2;
const int IR_EXIT_PIN       = 3;

HX711 scale;
float calibration_factor = 400.0;

// simple FIFO queue for entered vehicle weights (max 30 vehicles queued)
const int MAX_QUEUE = 30;
float weightsQueue[MAX_QUEUE];
int q_head = 0; // index of first valid
int q_tail = 0; // one past last valid
int vehicleCount = 0;
float currentBridgeWeight = 0.0;

unsigned long lastSerialMillis = 0;

void enqueueWeight(float w) {
  if ((q_tail + 1) % MAX_QUEUE == q_head) {
    // queue full - drop oldest to make room
    q_head = (q_head + 1) % MAX_QUEUE;
  }
  weightsQueue[q_tail] = w;
  q_tail = (q_tail + 1) % MAX_QUEUE;
}

float dequeueWeight() {
  if (q_head == q_tail) return 0.0; // empty
  float w = weightsQueue[q_head];
  q_head = (q_head + 1) % MAX_QUEUE;
  return w;
}

bool queueEmpty() {
  return q_head == q_tail;
}

void setup() {
  Serial.begin(9600);
  delay(200);
  Serial.println(F("{\"system\":\"Bridge_Project\",\"status\":\"boot\"}"));

  pinMode(IR_ENTRY_PIN, INPUT);
  pinMode(IR_EXIT_PIN, INPUT);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  Serial.println(F("Taring scale - ensure no weight on scale..."));
  scale.tare();

  randomSeed(analogRead(A0));
  delay(200);
  Serial.println(F("{\"system\":\"ready\"}"));
}

float readWeight() {
  // average of 5 reads
  float w = scale.get_units(5);
  if (w < 0) w = 0.0;
  return w;
}

float simVibrationEnter() {
  // small vibration between 0.01g - 0.08g
  return (float)random(10, 80) / 1000.0;
}
float simTiltEnter() {
  // tilt -1.5 to 1.5 deg
  return (float)random(-15, 15) / 10.0;
}
float simVibrationExit() {
  return (float)random(0, 30) / 1000.0;
}
float simTiltExit() {
  return (float)random(-5, 5) / 10.0;
}

void printJsonEvent(const char* ev, float vehicleWeight, float vibration, float tilt) {
  // Timestamp
  unsigned long ms = millis();
  // Build simple JSON (no spaces) and print as single line
  // fields: event, ts_ms, vehicleWeight, vibration, tilt, vehicleCount, totalLoad
  Serial.print("{\"event\":\"");
  Serial.print(ev);
  Serial.print("\",\"ts_ms\":");
  Serial.print(ms);
  Serial.print(",\"vehicleWeight\":");
  Serial.print(vehicleWeight, 1);
  Serial.print(",\"vibration\":");
  Serial.print(vibration, 3);
  Serial.print(",\"tilt\":");
  Serial.print(tilt, 2);
  Serial.print(",\"vehicleCount\":");
  Serial.print(vehicleCount);
  Serial.print(",\"totalLoad\":");
  Serial.print(currentBridgeWeight, 1);
  Serial.println("}");
}

void loop() {
  // Entry detection: IR sensor usually goes LOW on detection
  if (digitalRead(IR_ENTRY_PIN) == LOW) {
    delay(350); // settle
    float w = readWeight();
    if (w < 0) w = 0;
    vehicleCount++;
    currentBridgeWeight += w;
    enqueueWeight(w);
    float vib = simVibrationEnter();
    float tilt = simTiltEnter();
    printJsonEvent("enter", w, vib, tilt);

    // block until sensor clears
    while (digitalRead(IR_ENTRY_PIN) == LOW) delay(50);
    delay(350); // debounce
  }

  // Exit detection
  if (digitalRead(IR_EXIT_PIN) == LOW) {
    delay(250); // settle
    float popped = 0.0;
    if (!queueEmpty()) {
      popped = dequeueWeight();
      vehicleCount = max(0, vehicleCount - 1);
      currentBridgeWeight -= popped;
      if (currentBridgeWeight < 0) currentBridgeWeight = 0;
    } else {
      // If queue empty, attempt to read scale (best-effort)
      float measured = readWeight();
      if (measured >= 0) {
        popped = measured;
        currentBridgeWeight = max(0.0, currentBridgeWeight - popped);
        vehicleCount = max(0, vehicleCount - 1);
      }
    }
    float vib = simVibrationExit();
    float tilt = simTiltExit();
    printJsonEvent("exit", popped, vib, tilt);

    // block until sensor clears
    while (digitalRead(IR_EXIT_PIN) == LOW) delay(50);
    delay(300);
  }

  // small idle delay
  delay(30);
}
