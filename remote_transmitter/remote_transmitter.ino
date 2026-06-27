/*
 * UV-C SANITIZATION ROBOT — TRANSMITTER (REMOTE UNIT)
 * =====================================================
 * Hardware: ESP32 (30-pin DevKit)
 * Protocol: ESP-NOW (peer-to-peer, no WiFi router needed)
 * Course:   21ECC301P — SRM Institute of Science and Technology
 *
 * PIN MAPPING:
 *   GPIO33 — Joystick VRX (X-axis: Left/Right)
 *   GPIO32 — Joystick VRY (Y-axis: Forward/Backward)
 *   GPIO13 — Push Button: Solenoid/Pump toggle
 *   GPIO14 — Push Button: Brush Motor toggle
 *   GPIO27 — Push Button: UV-C Lamp toggle
 *
 * COMMAND PROTOCOL (single char sent via ESP-NOW):
 *   'F' — Forward   'B' — Backward
 *   'L' — Left      'R' — Right
 *   'S' — Stop
 *   '6'/'a' — Pump  ON/OFF
 *   '7'/'b' — Brush ON/OFF
 *   '8'/'c' — UV    ON/OFF
 *
 * IMPORTANT: Update receiverMac[] with your ESP8266 robot's MAC address.
 * To find it: flash robot_receiver.ino first, open Serial Monitor,
 * read the MAC printed at startup.
 *
 * BOARD SETTINGS (Arduino IDE):
 *   Board: ESP32 Dev Module
 *   Upload Speed: 921600
 *   Flash Frequency: 80MHz
 */

#include <WiFi.h>
#include <esp_now.h>

// ─── Receiver MAC Address ─────────────────────────────────────────────────────
// UPDATE THIS with your ESP8266 robot's MAC address
uint8_t receiverMac[6] = {0x8C, 0xAA, 0xB5, 0x16, 0x5F, 0x8A};

// ─── Pin Definitions ──────────────────────────────────────────────────────────
#define VRX_PIN      33   // Joystick X-axis (Left/Right)
#define VRY_PIN      32   // Joystick Y-axis (Forward/Backward)
#define BTN_SOLENOID 13   // Water pump toggle button
#define BTN_MOTOR    14   // Brush motor toggle button
#define BTN_UV       27   // UV-C lamp toggle button

// ─── Joystick Calibration ─────────────────────────────────────────────────────
int centerX, centerY;
const int THRESHOLD = 400;   // Dead-zone around center (0–4095 range)

// ─── State Tracking ───────────────────────────────────────────────────────────
char lastMotionCmd  = 'S';   // Last motion command sent

bool solenoidState  = false;
bool motorState     = false;
bool uvState        = false;

bool lastSolenoidBtn = HIGH;
bool lastMotorBtn    = HIGH;
bool lastUvBtn       = HIGH;

// ─── ESP-NOW Send ─────────────────────────────────────────────────────────────
void sendCommand(char cmd) {
    esp_err_t result = esp_now_send(receiverMac, (uint8_t*)&cmd, 1);
    if (result == ESP_OK) {
        Serial.printf("Sent: %c\n", cmd);
    } else {
        Serial.printf("Send FAILED: %c (err=%d)\n", cmd, result);
    }
}

// ─── ESP-NOW Send Callback (optional debug) ───────────────────────────────────
void onSent(const uint8_t* mac, esp_now_send_status_t status) {
    // Uncomment for verbose delivery confirmation:
    // Serial.printf("Delivery: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\nUV-C Robot Transmitter — Starting...");

    // Configure pins
    pinMode(VRX_PIN, INPUT);
    pinMode(VRY_PIN, INPUT);
    pinMode(BTN_SOLENOID, INPUT_PULLUP);
    pinMode(BTN_MOTOR,    INPUT_PULLUP);
    pinMode(BTN_UV,       INPUT_PULLUP);

    // ESP-NOW init
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init FAILED — halting");
        while (true) { delay(1000); }
    }

    esp_now_register_send_cb(onSent);

    // Register receiver peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer — check MAC address");
        while (true) { delay(1000); }
    }

    // Joystick center calibration
    Serial.println("Keep joystick centered — calibrating in 2s...");
    delay(2000);
    centerX = analogRead(VRX_PIN);
    centerY = analogRead(VRY_PIN);
    Serial.printf("Calibrated: centerX=%d, centerY=%d\n", centerX, centerY);
    Serial.println("Remote Ready!");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {

    // ── Joystick Motion Control ──────────────────────────────────────────────
    int x = analogRead(VRX_PIN);
    int y = analogRead(VRY_PIN);
    char motionCmd = 'S';

    if      (y < centerY - THRESHOLD) motionCmd = 'F';  // Push forward
    else if (y > centerY + THRESHOLD) motionCmd = 'B';  // Pull backward
    else if (x < centerX - THRESHOLD) motionCmd = 'L';  // Push left
    else if (x > centerX + THRESHOLD) motionCmd = 'R';  // Push right
    else                               motionCmd = 'S';  // Center = stop

    // Only send on change to avoid flooding ESP-NOW
    if (motionCmd != lastMotionCmd) {
        sendCommand(motionCmd);
        lastMotionCmd = motionCmd;

        // Auto-disable UV when motion starts (safety)
        if (motionCmd != 'S' && uvState) {
            sendCommand('c');   // UV OFF
            uvState = false;
            Serial.println("UV auto-OFF (motion started)");
        }
    }

    // ── Push Button: Water Pump (Solenoid) ───────────────────────────────────
    bool solenoidBtn = digitalRead(BTN_SOLENOID);
    if (lastSolenoidBtn == HIGH && solenoidBtn == LOW) {
        solenoidState = !solenoidState;
        sendCommand(solenoidState ? '6' : 'a');
        Serial.printf("Pump %s\n", solenoidState ? "ON" : "OFF");
        delay(200);  // Debounce
    }
    lastSolenoidBtn = solenoidBtn;

    // ── Push Button: Brush Motor ──────────────────────────────────────────────
    bool motorBtn = digitalRead(BTN_MOTOR);
    if (lastMotorBtn == HIGH && motorBtn == LOW) {
        motorState = !motorState;
        sendCommand(motorState ? '7' : 'b');
        Serial.printf("Brush %s\n", motorState ? "ON" : "OFF");
        delay(200);
    }
    lastMotorBtn = motorBtn;

    // ── Push Button: UV-C Lamp ────────────────────────────────────────────────
    bool uvBtn = digitalRead(BTN_UV);
    if (lastUvBtn == HIGH && uvBtn == LOW) {
        // Safety: block UV if robot is moving
        if (!uvState && lastMotionCmd != 'S') {
            Serial.println("UV BLOCKED — stop robot first");
        } else {
            uvState = !uvState;
            sendCommand(uvState ? '8' : 'c');
            Serial.printf("UV %s\n", uvState ? "ON" : "OFF");
        }
        delay(200);
    }
    lastUvBtn = uvBtn;

    delay(50);  // ~20Hz polling rate
}
