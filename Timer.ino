bool flashing = false;
bool flashState = false;
int flashCount = 0;
int flashR, flashG, flashB;
unsigned long lastFlashTime = 0;
const int FLASH_INTERVAL = 500;
const int FLASH_TOTAL = 20;
extern bool gameOver;

// ================= TIMER =================
void updateTimerRing() {
  if (!timerActive) return;
  if (millis() - lastTimerUpdate < 1500) return;

  lastTimerUpdate = millis();

  unsigned long elapsed = millis() - gameTimerStart;
  if (elapsed >= TIMER_DURATION) { // GAME LOST
    gameLost();
    return;
  }
  else if (gameOver && elapsed < TIMER_DURATION) { // GAME WON
    return;
  }

  float progress = (float)elapsed / TIMER_DURATION;
  int ledsPassed = (int)(progress * 16.0 + 0.5);
  if (ledsPassed > 16) ledsPassed = 16;

  int r = 255 * progress;
  int g = 255 * (1.0 - progress);

  for (int i = 0; i < 16; i++) {
    int idx = (i + 8) % 16;

    if (i < ledsPassed) {
      ring.setPixelColor(idx, 0); // OFF instead of red (stable)
    } else {
      ring.setPixelColor(idx, ring.Color(r,g,0));
    }
  }

  ring.show();
}

void gameLost() {
  strcpy(incomingData.message, "Game_Over");
  esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
  gameStarted = false;
  timerActive = false;
  gameOver = true;
  startFlash(255, 0, 0);
  Serial.println("Game Over!");
}

void gameWon() {
  gameStarted = false;
  timerActive = false;
  gameOver = true;
  startFlash(0, 255, 0);
  Serial.println("Game Over!");
}

void startFlash(int r, int g, int b) {
  flashing = true;
  flashState = true;
  flashCount = 0;
  flashR = r;
  flashG = g;
  flashB = b;
  lastFlashTime = millis();
}

void flashTimerRing(int r, int g, int b) {
  for (int i = 0; i < 16; i++) {
    ring.setPixelColor(i, ring.Color(r, g, b));
  }
  ring.show();
}

void updateFlash() {
  if (!flashing) return;
  if (millis() - lastFlashTime < FLASH_INTERVAL) return;

  lastFlashTime = millis();

  int r = flashState ? 0 : flashR;
  int g = flashState ? 0 : flashG;
  int b = flashState ? 0 : flashB;

  for (int i = 0; i < 16; i++) {
    ring.setPixelColor(i, ring.Color(r,g,b));
  }
  ring.show();

  flashState = !flashState;
  flashCount++;

  if (flashCount >= FLASH_TOTAL) {
    if (gameOver) { // RESTART ESP32 TO RESET THE WHOLE UNIT
      ESP.restart();
      return;
    }

    flashing = false;
  }
}