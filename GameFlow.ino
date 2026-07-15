// ================= PUZZLE STATES =================
bool fusePuzzleUnlocked   = false;
bool symbolPuzzleUnlocked = false;
bool mazePuzzleUnlocked   = false;

// ================= GAME STATES =================
bool gameStarted = false;
bool gameOver = false;
bool easyMode = true;

// ================= VARIABLES =================
extern SimpleTimer motorActionTimer;

// ================= TIMER =================
bool timerActive = false;
unsigned long gameTimerStart = 0;
const unsigned long TIMER_DURATION = 600000;
unsigned long lastTimerUpdate = 0;

int pulseBrightness = 0;
int pulseDirection = 1;

unsigned long blinkTimer = 0;
bool blinkState = false;
extern bool isHomed;

void clearReadyPulse()
{
    for (int i = 0; i < LEDS_IN_RING; i++)
    {
        ring.setPixelColor(i, 0);
    }

    ring.show();
}

void updateReadyPulse()
{
    if (!readyUpActive)
        return;

    unsigned long elapsed = millis() - readyUpStartTime;

    // 0 - 20 sec
    if (elapsed < 20000)
    {
        pulseBrightness += pulseDirection * 2;

        if (pulseBrightness >= 180)
            pulseDirection = -1;

        if (pulseBrightness <= 20)
            pulseDirection = 1;

        for (int i = 0; i < LEDS_IN_RING; i++)
        {
            ring.setPixelColor(
                i,
                ring.Color(
                    pulseBrightness,
                    0,
                    pulseBrightness));
        }

        ring.show();
    }

    // 20 - 25 sec
    else if (elapsed < 25000)
    {
        pulseBrightness += pulseDirection * 6;

        if (pulseBrightness >= 255)
            pulseDirection = -1;

        if (pulseBrightness <= 10)
            pulseDirection = 1;

        for (int i = 0; i < LEDS_IN_RING; i++)
        {
            ring.setPixelColor(
                i,
                ring.Color(
                    pulseBrightness,
                    0,
                    pulseBrightness));
        }

        ring.show();
    }

    // 25 - 30 sec
    else
    {
        if (millis() - blinkTimer > 150)
        {
            blinkTimer = millis();

            blinkState = !blinkState;

            for (int i = 0; i < LEDS_IN_RING; i++)
            {
                ring.setPixelColor(
                    i,
                    blinkState
                        ? ring.Color(255, 0, 0)
                        : ring.Color(0, 0, 0));
            }

            ring.show();
        }
    }
}

void startReadyUp(int mode)
{
    if (readyUpActive || gameStarted || !isHomed || millis() < 10000) return;

    selectedMode = mode;

    readyUpActive = true;

    readyUpStartTime = millis();

    if (selectedMode == 0) {
        strcpy(incomingData.message, "Ready_Up_Easy");
    } else {
        strcpy(incomingData.message, "Ready_Up_Hard");
    }

    esp_now_send(
        SecondCaseAddress,
        (uint8_t*)&incomingData,
        sizeof(incomingData));

    Serial.println("Waiting for ready confirmation");
}

void updateReadyUp()
{
    if (!readyUpActive || gameStarted || gameOver)
        return;

    if (millis() - readyUpStartTime >= READY_UP_TIMEOUT)
    {
        readyUpActive = false;

        clearReadyPulse();

        strcpy(incomingData.message,
               "Ready_Up_Cancelled");

        esp_now_send(
            SecondCaseAddress,
            (uint8_t*)&incomingData,
            sizeof(incomingData));

        Serial.println("Ready-up timeout");
    }
}

void startGame(int mode) {
  if (gameStarted || timerActive) return;
  strcpy(incomingData.message, "Game_Started");
  esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
  gameStarted = true;
  fusePuzzleUnlocked = true;
  wipeServos();
  easyMode = (mode == 0);
  timerActive = true;
  gameTimerStart = millis();
  Serial.println(easyMode ? "Easy Mode Started!" : "Hard Mode Started!");
}

void fusePuzzleSolved() {
    if (gameOver) return;
  strcpy(incomingData.message, "Fuse_Puzzle_Solved");
  esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
  fusePuzzleUnlocked = false;
  symbolPuzzleUnlocked = true;
  motorActionTimer.setTimeout(2000, afterOffsetWait); // Show puzzle screen for symbol puzzle
  Serial.println("Fuse Puzzle Solved!");
  Serial.println("Symbol Puzzle Unlocked!");
}

void symbolPuzzleSolved() {
    if (gameOver) return;
  symbolPuzzleUnlocked = false;
  mazePuzzleUnlocked = true;
  motorActionTimer.setTimeout(3000, moveBackToHome); // Hide puzzle screen, because it's solved
  Serial.println("Symbol Puzzle Solved!");
  Serial.println("Maze Puzzle Unlocked!");
}

void mazePuzzleSolved() {
    if (gameOver) return;
  strcpy(incomingData.message, "Maze_Puzzle_Solved");
  esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
  mazePuzzleUnlocked = false;
  gameOver = true;
  Serial.println("Maze Puzzle Solved!");
}