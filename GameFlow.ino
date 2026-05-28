// ================= PUZZLE STATES =================
bool fusePuzzleUnlocked   = true;
bool symbolPuzzleUnlocked = true;
bool mazePuzzleUnlocked   = false;

// ================= GAME STATES =================
bool gameStarted = false;
bool easyMode = true;

// ================= VARIABLES =================
extern SimpleTimer motorActionTimer;

// ================= TIMER =================
// ================= TIMER =================
bool timerActive = false;
unsigned long gameTimerStart = 0;
const unsigned long TIMER_DURATION = 100000;
unsigned long lastTimerUpdate = 0;

void startGame(int mode) {
  strcpy(incomingData.message, "Game_Started");
  esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
  gameStarted = true;
  easyMode = (mode == 0);
  timerActive = true;
  gameTimerStart = millis();
  Serial.println(easyMode ? "Easy Mode Started!" : "Hard Mode Started!");
}

void fusePuzzleSolved() {
  strcpy(incomingData.message, "Fuse_Puzzle_Solved");
  esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
  fusePuzzleUnlocked = false;
  symbolPuzzleUnlocked = true;
  motorActionTimer.setTimeout(2000, afterOffsetWait); // Show puzzle screen for symbol puzzle
  Serial.println("Fuse Puzzle Solved!");
  Serial.println("Symbol Puzzle Unlocked!");
}

void symbolPuzzleSolved() {
  symbolPuzzleUnlocked = false;
  mazePuzzleUnlocked = true;
  motorActionTimer.setTimeout(3000, moveBackToHome); // Hide puzzle screen, because it's solved
  Serial.println("Symbol Puzzle Solved!");
  Serial.println("Maze Puzzle Unlocked!");
}

void mazePuzzleSolved() {
  strcpy(incomingData.message, "Maze_Puzzle_Solved");
  esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
  mazePuzzleUnlocked = false;
  Serial.println("Maze Puzzle Solved!");
}