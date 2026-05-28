// ================= TIMER =================
void updateTimerRing() {
  if (!timerActive) return;
  if (millis() - lastTimerUpdate < 1500) return;

  lastTimerUpdate = millis();

  unsigned long elapsed = millis() - gameTimerStart;
  if (elapsed >= TIMER_DURATION) return;

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