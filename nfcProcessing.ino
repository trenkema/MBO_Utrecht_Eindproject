uint8_t uidReader1[7] = {0x04, 0xC7, 0x21, 0x9E, 0xCC, 0x2A, 0x81}; 
uint8_t uidReader2[7] = {0x04, 0x43, 0x11, 0x9E, 0xCC, 0x2A, 0x81};

struct SlotState {
  bool present;
  bool correct;
  bool almost;
};

SlotState slot1 = {false, false, false};
SlotState slot2 = {false, false, false};

SlotState lastSlot1 = {false, false, false};
SlotState lastSlot2 = {false, false, false};

bool compareUID(uint8_t *uid, uint8_t len, uint8_t *target) {
  if (len != 7) return false;

  for (int i = 0; i < 7; i++) {
    if (uid[i] != target[i]) return false;
  }
  return true;
}

void nfcTask() {
  if (!fusePuzzleUnlocked) return; // Only run if puzzle not solved yet
  uint8_t uid[7];
  uint8_t uid2[7];
  uint8_t uidLength;
  uint8_t uidLength2;

  // reset current state
  slot1 = {false, false, false};
  slot2 = {false, false, false};

  // ---- Reader 1 ----
  if (nfc3.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 30)) {
    Serial.println("Reader 1 detected!");
    slot1.present = true;

    if (compareUID(uid, uidLength, uidReader1)) {
      slot1.correct = true;
    } 
    else if (compareUID(uid, uidLength, uidReader2)) {
      slot1.almost = true;
    }
  }

  // ---- Reader 2 ----
  if (nfc2.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid2, &uidLength2, 30)) {
    Serial.println("Reader 2 detected!");
    slot2.present = true;

    if (compareUID(uid2, uidLength2, uidReader2)) {
      slot2.correct = true;
    } 
    else if (compareUID(uid2, uidLength2, uidReader1)) {
      slot2.almost = true;
    }
  }

  // ---- GLOBAL STATE ----
  bool bothPresent = slot1.present && slot2.present;
  bool lastBothPresent = lastSlot1.present && lastSlot2.present;

  // ---- GLOBAL CHANGE HANDLING ----
  if (bothPresent != lastBothPresent) {

    if (!bothPresent) {
      Serial.println("🚫 One removed → send OFF");

      strcpy(incomingData.message, "Fuse_All_Off");
      esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
    } 
    else {
      Serial.println("✅ Both inserted → resend BOTH states");

      // ---- SLOT 1 ----
      if (slot1.correct) strcpy(incomingData.message, "Fuse_1_Correct");
      else if (slot1.almost) strcpy(incomingData.message, "Fuse_1_Almost");
      else strcpy(incomingData.message, "Fuse_1_Incorrect");

      esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));

      // ---- SLOT 2 ----
      if (slot2.correct) strcpy(incomingData.message, "Fuse_2_Correct");
      else if (slot2.almost) strcpy(incomingData.message, "Fuse_2_Almost");
      else strcpy(incomingData.message, "Fuse_2_Incorrect");

      esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
    }
  }

  // ---- SLOT UPDATES (ONLY IF BOTH PRESENT) ----
  if (bothPresent) {

    // ---- SLOT 1 CHANGED ----
    if (slot1.present != lastSlot1.present ||
        slot1.correct != lastSlot1.correct ||
        slot1.almost  != lastSlot1.almost) {

      if (slot1.correct) strcpy(incomingData.message, "Fuse_1_Correct");
      else if (slot1.almost) strcpy(incomingData.message, "Fuse_1_Almost");
      else strcpy(incomingData.message, "Fuse_1_Incorrect");

      esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
    }

    // ---- SLOT 2 CHANGED ----
    if (slot2.present != lastSlot2.present ||
        slot2.correct != lastSlot2.correct ||
        slot2.almost  != lastSlot2.almost) {

      if (slot2.correct) strcpy(incomingData.message, "Fuse_2_Correct");
      else if (slot2.almost) strcpy(incomingData.message, "Fuse_2_Almost");
      else strcpy(incomingData.message, "Fuse_2_Incorrect");

      esp_now_send(SecondCaseAddress, (uint8_t *) &incomingData, sizeof(incomingData));
    }

    if (slot1.correct && slot2.correct) {
      fusePuzzleSolved();
      Serial.println("🎉 Both correct → Puzzle solved!");
    }
  }

  // ---- SAVE LAST STATE ----
  lastSlot1 = slot1;
  lastSlot2 = slot2;
}