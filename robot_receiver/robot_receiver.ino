/*
 * UV-C SANITIZATION ROBOT — RECEIVER (ROBOT UNIT)
 * ================================================
 * Hardware: ESP8266 NodeMCU
 * Protocol: ESP-NOW (peer-to-peer, no WiFi router needed)
 * Course:   21ECC301P — SRM Institute of Science and Technology
 *
 * RELAY PIN MAPPING:
 *   RELAY1 (GPIO16) — Motor Left  Forward
 *   RELAY2 (GPIO14) — Motor Left  Backward
 *   RELAY3 (GPIO12) — Motor Right Forward
 *   RELAY4 (GPIO13) — Motor Right Backward
 *   RELAY5 (GPIO15) — Motor Enable / Speed
 *   RELAY6 (GPIO0)  — Water Pump (Solenoid)
 *   RELAY7 (GPIO4)  — Brush Motor
 *   RELAY8 (GPIO5)  — UV-C Lamp
 *
 * COMMAND PROTOCOL (single char from transmitter):
 *   'F' — Move Forward
 *   'B' — Move Backward
 *   'L' — Turn Left
 *   'R' — Turn Right
 *   'S' — Stop all motors
 *   '6' — Solenoid/Pump ON    |  'a' — Solenoid/Pump OFF
 *   '7' — Brush Motor ON      |  'b' — Brush Motor OFF
 *   '8' — UV-C Lamp ON        |  'c' — UV-C Lamp OFF
 *
 * SAFETY NOTE: UV lamp ('8') should only be activated when
 * the robot is stationary ('S' command active). This is
 * enforced in the transmitter but also checked here.
 *
 * BOARD SETTINGS (Arduino IDE):
 *   Board: NodeMCU 1.0 (ESP-12E Module)
 *   Upload Speed: 115200
 */

#include <ESP8266WiFi.h>
#include <espnow.h>

// ─── Relay Pin Definitions ───────────────────────────────────────────────────
const int RELAY1 = 16;  // Motor Left  Forward
const int RELAY2 = 14;  // Motor Left  Backward
const int RELAY3 = 12;  // Motor Right Forward
const int RELAY4 = 13;  // Motor Right Backward
const int RELAY5 = 15;  // Motor Enable
const int RELAY6 = 0;   // Water Pump (Solenoid)
const int RELAY7 = 4;   // Brush Motor
const int RELAY8 = 5;   // UV-C Lamp

// Active-LOW relay board (most common 8-ch relay modules)
// Set to false if your relay board is Active-HIGH
const bool ACTIVE_LOW = true;

// Track motion state — UV lamp must not run while moving
bool isMoving = false;
bool uvState  = false;

// ─── Relay Helper ────────────────────────────────────────────────────────────
void setRelay(int pin, bool on) {
    digitalWrite(pin, ACTIVE_LOW ? !on : on);
}

// ─── Motion Commands ─────────────────────────────────────────────────────────
void stopAll() {
    setRelay(RELAY1, false);
    setRelay(RELAY2, false);
    setRelay(RELAY3, false);
    setRelay(RELAY4, false);
    setRelay(RELAY5, false);
    isMoving = false;
    Serial.println("STOP");
}

void moveForward() {
    // Safety: cut UV before moving
    if (uvState) {
        setRelay(RELAY8, false);
        Serial.println("UV OFF (safety — motion detected)");
    }
    setRelay(RELAY1, true);   // Left  motor forward
    setRelay(RELAY2, false);
    setRelay(RELAY3, true);   // Right motor forward
    setRelay(RELAY4, false);
    setRelay(RELAY5, true);   // Enable
    isMoving = true;
    Serial.println("FORWARD");
}

void moveBackward() {
    if (uvState) {
        setRelay(RELAY8, false);
        Serial.println("UV OFF (safety — motion detected)");
    }
    setRelay(RELAY1, false);
    setRelay(RELAY2, true);   // Left  motor backward
    setRelay(RELAY3, false);
    setRelay(RELAY4, true);   // Right motor backward
    setRelay(RELAY5, true);   // Enable
    isMoving = true;
    Serial.println("BACKWARD");
}

void turnLeft() {
    if (uvState) {
        setRelay(RELAY8, false);
        Serial.println("UV OFF (safety — motion detected)");
    }
    setRelay(RELAY1, false);
    setRelay(RELAY2, true);   // Left  motor backward
    setRelay(RELAY3, true);   // Right motor forward
    setRelay(RELAY4, false);
    setRelay(RELAY5, true);   // Enable
    isMoving = true;
    Serial.println("LEFT");
}

void turnRight() {
    if (uvState) {
        setRelay(RELAY8, false);
        Serial.println("UV OFF (safety — motion detected)");
    }
    setRelay(RELAY1, true);   // Left  motor forward
    setRelay(RELAY2, false);
    setRelay(RELAY3, false);
    setRelay(RELAY4, true);   // Right motor backward
    setRelay(RELAY5, true);   // Enable
    isMoving = true;
    Serial.println("RIGHT");
}

// ─── ESP-NOW Receive Callback ─────────────────────────────────────────────────
void onReceive(uint8_t* mac, uint8_t* data, uint8_t len) {
    if (len < 1) return;

    char cmd = (char)data[0];
    Serial.printf("CMD: %c\n", cmd);

    switch (cmd) {
        // Motion
        case 'F': moveForward();  break;
        case 'B': moveBackward(); break;
        case 'L': turnLeft();     break;
        case 'R': turnRight();    break;
        case 'S': stopAll();      break;

        // Solenoid / Water Pump
        case '6':
            setRelay(RELAY6, true);
            Serial.println("PUMP ON");
            break;
        case 'a':
            setRelay(RELAY6, false);
            Serial.println("PUMP OFF");
            break;

        // Brush Motor
        case '7':
            setRelay(RELAY7, true);
            Serial.println("BRUSH ON");
            break;
        case 'b':
            setRelay(RELAY7, false);
            Serial.println("BRUSH OFF");
            break;

        // UV-C Lamp — only allow when stationary
        case '8':
            if (!isMoving) {
                setRelay(RELAY8, true);
                uvState = true;
                Serial.println("UV ON");
            } else {
                Serial.println("UV BLOCKED — robot moving");
            }
            break;
        case 'c':
            setRelay(RELAY8, false);
            uvState = false;
            Serial.println("UV OFF");
            break;

        default:
            Serial.printf("Unknown CMD: %c\n", cmd);
            break;
    }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\nUV-C Robot Receiver — Starting...");

    // Initialize all relay pins
    int pins[] = {RELAY1, RELAY2, RELAY3, RELAY4,
                  RELAY5, RELAY6, RELAY7, RELAY8};
    for (int i = 0; i < 8; i++) {
        pinMode(pins[i], OUTPUT);
        setRelay(pins[i], false);  // All OFF at startup
    }

    // ESP-NOW init
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != 0) {
        Serial.println("ESP-NOW init FAILED — halting");
        while (true) { delay(1000); }
    }

    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(onReceive);

    Serial.println("ESP-NOW Ready — Listening for commands...");
    Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    // All logic handled in callback
    // Small delay prevents watchdog reset
    delay(10);
}
