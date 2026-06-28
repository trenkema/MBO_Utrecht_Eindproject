void processMessages(const uint8_t * mac, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  
  if (gameStarted &&strcmp(incomingData.message, "Game_Won") == 0) gameWon();
  if (strcmp(incomingData.message, "Ready_Confirmed") == 0 && readyUpActive) {
    Serial.println("READY CONFIRMED RECEIVED");

    readyUpActive = false;

    pendingGameStart = true;
  }
  if (symbolPuzzleUnlocked) symbolMessages(incomingData.message);
  if (mazePuzzleUnlocked) mazeMessages(incomingData.message);
}

void symbolMessages(const char* message) {
  if (strcmp(message, "Symbol_Puzzle_Solved") == 0) {
    symbolPuzzleSolved();
  }
}

void mazeMessages(const char* message) {
}