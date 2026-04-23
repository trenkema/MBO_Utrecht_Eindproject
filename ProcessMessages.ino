void processMessages(const uint8_t * mac, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  
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