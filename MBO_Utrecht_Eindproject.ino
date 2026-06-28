#include <SPI.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_PWMServoDriver.h>
#include <AS5600.h>
#include <SimpleTimer.h>
#include <HC4067.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_now.h>

void updateTimerRing();
void motorState();
void setupMotor();
void updateMotor();
void nfcTask();

// ---------- READY UP SYSTEM ----------
bool readyUpActive = false;
int selectedMode = 0;
unsigned long readyUpStartTime = 0;
const unsigned long READY_UP_TIMEOUT = 30000;
volatile bool pendingGameStart = false;

// ---------- NEOPIXEL RING ----------
#define LEDS_IN_RING 16
#define COUNTDOWN_INTERVAL 3750  // 60s / 16

int countdownLED = LEDS_IN_RING;
int countdownTimerID = -1;

#define PIXEL_PIN 32
#define NUMPIXELS 16
Adafruit_NeoPixel ring(NUMPIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel talk_led(1, 25, NEO_GRB + NEO_KHZ800);

void ringColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUMPIXELS; i++)
    ring.setPixelColor(i, ring.Color(r, g, b));
  ring.show();
}

// ---------- RGB LED ON PCA ----------
#define RGB_R 2
#define RGB_G 3
#define RGB_B 4

void lightsOff() {
  setRGB(0, 0, 0);
  ringColor(0, 0, 0);
}

// ---------- TIMERS ----------
SimpleTimer nfcTimer;
SimpleTimer nfcActionTimer;
SimpleTimer randomActionTimer;

// ---------- PCA SERVO ----------
Adafruit_PWMServoDriver pca(0x40);
#define SERVO_MIN 150
#define SERVO_MAX 600

bool isOtherTalking = false;
bool isTalking = false;

// ---------- ESP-NOW CALLBACKS ----------
// 👉 MAC adressen van je slaves (AANPASSEN!)
uint8_t SecondCaseAddress[] = {0x2C, 0xBC, 0xBB, 0x06, 0x27, 0x18};
uint8_t MazeAddress[] = {0xB0, 0xCB, 0xD8, 0xCD, 0xE8, 0x40};

// 👉 Struct (moet exact hetzelfde zijn op slaves!)
typedef struct {
  char message[32];
} Message;

Message incomingData;

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  pca.setPWM(RGB_R, 0, map(r, 0, 255, 0, 4095));
  pca.setPWM(RGB_G, 0, map(g, 0, 255, 0, 4095));
  pca.setPWM(RGB_B, 0, map(b, 0, 255, 0, 4095));
}

void setServoAngle(uint8_t ch, int angle) {
  uint16_t pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pca.setPWM(ch, 0, pulse);
}

void resetServos() {
  setServoAngle(0, 2);
  setServoAngle(1, 2);
}

void wipeServos() {
  setServoAngle(0, 57);
  setServoAngle(1, 57);
  randomActionTimer.setTimeout(2000, resetServos);
}

// ---------- PN532 ----------
#define PN532_SCK 18
#define PN532_MISO 19
#define PN532_MOSI 23
#define CS2 16
#define CS3 17

Adafruit_PN532 nfc2(CS2);
Adafruit_PN532 nfc3(CS3);

// ---------- MUX BUTTONS ----------
HC4067 mux(13, 4, 2, 15);
#define MUX_SIG 35
bool lastButtonState[3] = { HIGH, HIGH, HIGH };

bool readButton(int ch) {
  mux.setChannel(ch);
  delayMicroseconds(3);
  return digitalRead(MUX_SIG) == LOW;
}

void countdownStep() {
  if (countdownLED <= 0) {
    ring.clear();
    ring.show();
    nfcActionTimer.deleteTimer(countdownTimerID);
    countdownTimerID = -1;
    return;
  }

  ring.setPixelColor(countdownLED - 1, 0);  // turn off 1 LED
  ring.show();
  countdownLED--;
}

void startCountdown() {
  ringColor(0, 0, 255);  // full blue
  countdownLED = LEDS_IN_RING;

  if (countdownTimerID != -1)
    nfcActionTimer.deleteTimer(countdownTimerID);

  countdownTimerID = nfcActionTimer.setInterval(COUNTDOWN_INTERVAL, countdownStep);
}

// ---------- BUTTON CHECK ----------
void checkButtons() {
  for (int i = 0; i < 3; i++) {
    bool pressed = readButton(i);
    if (pressed != lastButtonState[i]) {
      lastButtonState[i] = pressed;
      if (pressed) {
        Serial.print("Button ");
        Serial.print(i + 1);
        Serial.println(" pressed");

        if (i == 0) {  // Button 1 - Top
          talk_led.setPixelColor(0, talk_led.Color(0, 0, 0));
          talk_led.show();
          strcpy(incomingData.message, "NOT_TALKING");
          esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
          // startCountdown();
        }
        if (i == 1) {  // Button 3 - Bottom Left // Start Easy
          startReadyUp(0);
          // setServoAngle(1, 57);
          // nfcActionTimer.setTimeout(2000, resetServos);
        }

        if (i == 2) {  // BUTTON 2 - Bottom Right // Start Hard
          startReadyUp(1);
          // setServoAngle(0, 57);
          // nfcActionTimer.setTimeout(2000, resetServos);
        }
      }
      else if (!pressed && i == 0) {
        if (isOtherTalking) talk_led.setPixelColor(0, talk_led.Color(0, 255, 0)); // RED
        else talk_led.setPixelColor(0, talk_led.Color(255, 0, 0)); // GREEN
        talk_led.show();
        strcpy(incomingData.message, "TALKING");
        esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
      }
    }
  }
}

// 📤 Callback (verzenden status)
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// 📥 Callback (ontvangen van slaves)
void OnDataRecv(const uint8_t * mac, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));

  Serial.print("Message: ");
  Serial.print(incomingData.message);

  // TALKING STATUS
    // TALKING STATUS
  if (strcmp(incomingData.message, "TALKING") == 0) {
    isOtherTalking = true;
    if (!isTalking) talk_led.setPixelColor(0, talk_led.Color(0, 255, 0)); // RED
    talk_led.show();
  }
  else if (strcmp(incomingData.message, "NOT_TALKING") == 0) {
    isOtherTalking = false;
    if (isTalking) talk_led.setPixelColor(0, talk_led.Color(255, 0, 0)); // GREEN
    else talk_led.setPixelColor(0, talk_led.Color(0, 0, 0)); // OFF
    talk_led.show();
  }

  processMessages(mac, data, len);
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  SPI.begin(PN532_SCK, PN532_MISO, PN532_MOSI);

  pinMode(MUX_SIG, INPUT_PULLUP);

  pca.begin();
  pca.setPWMFreq(50);

  ring.begin();
  ring.setBrightness(5);
  ring.clear();
  ring.show();

  talk_led.begin();
  talk_led.setBrightness(15);
  talk_led.clear();
  talk_led.show();

  nfc2.begin();
  nfc3.begin();
  nfc2.SAMConfig();
  nfc3.SAMConfig();

  setupMotor();
  wipeServos();

  // ESP-NOW setup
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // 👉 Slave 1 toevoegen
  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, SecondCaseAddress, 6);
  peer1.channel = 0;
  peer1.encrypt = false;
  esp_now_add_peer(&peer1);

  // 👉 Slave 2 toevoegen
  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, MazeAddress, 6);
  peer2.channel = 0;
  peer2.encrypt = false;
  esp_now_add_peer(&peer2);

  Serial.println("Master ready!");
  nfcTimer.setInterval(500, nfcTask);
}

// ---------- LOOP ----------
void loop() {
  if (pendingGameStart)
  {
      pendingGameStart = false;

      clearReadyPulse();

      startGame(selectedMode);
  }
  updateTimerRing();
  updateMotor();
  nfcTimer.run();
  nfcActionTimer.run();
  randomActionTimer.run();
  checkButtons();
  updateReadyPulse();
  updateFlash();
}