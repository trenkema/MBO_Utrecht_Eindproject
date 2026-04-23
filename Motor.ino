// =========================
// TIMER
// =========================
hw_timer_t *stepTimer = NULL;
volatile bool stepLevel = false;
volatile bool stepEnabled = false;

// =========================
// STEPPER DRIVER
// =========================
#define STEP_PIN 26
#define DIR_PIN 14
#define HALL_PIN 33
#define EN_PIN 5

// =========================
// OBJECTS
// =========================
AS5600 as5600;
SimpleTimer motorActionTimer;

// =========================
// ANGLE VARIABLES
// =========================
float continuousAngle = 0, lastRaw = 0, zeroRef = 0;

#define ANGLE_TOL 3.0
#define ANGLE_HYST 1.5

float targets[] = {126, 246, 5};
int targetIndex = 0;
float targetAngle = 0;
float offsetAngle = 5;

bool holding = false;
bool inPosition = false;
bool forcedMove = false;

// =========================
// STATES
// =========================
enum State { HOMING, WAITING, OFFSET_MOVE, RUN };
State state = HOMING;

// =========================
// ENCODER FUNCTIONS
// =========================
float getContinuousAngle() {
  float raw = as5600.readAngle() * 360.0 / 4096.0;
  float delta = raw - lastRaw;

  if (delta > 180) delta -= 360;
  if (delta < -180) delta += 360;

  continuousAngle += delta;
  lastRaw = raw;

  return continuousAngle;
}

float wrap360(float a) {
  while (a >= 360) a -= 360;
  while (a < 0) a += 360;
  return a;
}

float getRelativeAngle() {
  float a = -(getContinuousAngle() - zeroRef);
  return wrap360(a);
}

// 🔥 Faster filter (less lag)
float getFilteredAngle() {
  static float filtered = 0;

  float raw = getRelativeAngle();
  float diff = shortestError(raw, filtered);

  filtered += diff * 0.3; // smoothing factor
  
  // 🔥 FIX: Actually wrap the static variable in memory!
  filtered = wrap360(filtered); 

  return filtered;
}

float shortestError(float target, float current) {
  float diff = fmod(target - current, 360.0);
  if (diff > 180.0) diff -= 360.0;
  if (diff < -180.0) diff += 360.0;
  return diff;
}

// =========================
// STEPPER CONTROL
// =========================
void stepTask() {

  static bool lastDir = false;
  static unsigned long lastDirChangeTime = 0;

  if (state == WAITING) {
    stepEnabled = false;
    digitalWrite(EN_PIN, HIGH);
    return;
  }

  if (state == HOMING) {
    digitalWrite(EN_PIN, LOW);

    if (!digitalRead(HALL_PIN)) {
      stepEnabled = false;
      return;
    }

    digitalWrite(DIR_PIN, HIGH);
    stepEnabled = true;
  }

  else if (state == OFFSET_MOVE || state == RUN) {

    digitalWrite(EN_PIN, LOW);

    float rawAngle = getRelativeAngle();
    float rawError = shortestError(targetAngle, rawAngle);

    // 🔥 Detect manual movement
    if (abs(rawError) > 8.0) {
      forcedMove = true;
      inPosition = false;
    }

    float current = getFilteredAngle();
    float error = shortestError(targetAngle, current);

    // Prevent false stop
    if (abs(error) < ANGLE_TOL && abs(rawError) > ANGLE_TOL) {
      inPosition = false;
    }

    // ENTER position
    if (!forcedMove && !inPosition && abs(error) < ANGLE_TOL) {
      inPosition = true;
      stepEnabled = false;
      return;
    }

    // EXIT position
    if (inPosition && abs(error) > 5.0) {
      inPosition = false;
    }

    // 🔥 MOVEMENT
    if (!inPosition) {

      bool newDir = (error > 0);

      // 🔥 Handle direction change properly
      if (newDir != lastDir) {
        digitalWrite(DIR_PIN, newDir);
        lastDir = newDir;
        lastDirChangeTime = micros();
        stepEnabled = false; // pause stepping briefly
        return;
      }

      // 🔥 Only step after DIR has settled
      if (micros() - lastDirChangeTime > 50) {
        stepEnabled = true;
      } else {
        stepEnabled = false;
      }

      // Exit forced mode when back
      if (forcedMove && abs(rawError) < ANGLE_TOL) {
        forcedMove = false;
      }

    } else {
      stepEnabled = false;
    }
  }
}

// =========================
// TIMER INTERRUPT
// =========================
void IRAM_ATTR onStepTimer() {
  if (!stepEnabled) return;

  if (state == HOMING && digitalRead(HALL_PIN) == LOW) {
    stepEnabled = false;
    return;
  }

  stepLevel = !stepLevel;
  digitalWrite(STEP_PIN, stepLevel);
}

// =========================
// CALLBACKS
// =========================
void afterHomeWait() {
  Serial.println("Moving to Offset...");
  targetAngle = offsetAngle;
  inPosition = false;
  state = OFFSET_MOVE;
}

void afterOffsetWait() { // Showing correct screen for symbol puzzle
  Serial.println("Entering RUN...");
  state = RUN;

  if (easyMode) {
    targetAngle = targets[1]; // Easy Screen
  } else {
    targetAngle = targets[0]; // Hard Screen
  }
//   targetAngle = targets[targetIndex];
  inPosition = false;
}

void moveBackToHome() { // Move back to empty screen
  holding = false;

  // 🔥 FIX: was %3 → should be %4
//   targetIndex = (targetIndex + 1) % 3;

  targetAngle = targets[2]; // Home Screen
  state = RUN;
  inPosition = false;

//   Serial.print("Next target: ");
//   Serial.println(targetAngle);
}

void motorState() {
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 500) {
        // Serial.print("State: "); Serial.print(state);
        // Serial.print(" | Angle: "); Serial.print(getRelativeAngle());
        // Serial.print(" | Target: "); Serial.println(targetAngle);
        lastDebug = millis();
    }

    switch (state) {

        case HOMING:
        if (digitalRead(HALL_PIN) == LOW) {

            stepEnabled = false;

            float raw = as5600.readAngle() * 360.0 / 4096.0;
            lastRaw = raw;
            continuousAngle = 0;
            zeroRef = 0;

            Serial.println("Homed!");

            state = WAITING;
            motorActionTimer.setTimeout(1000, afterHomeWait);
        }
        break;

        case OFFSET_MOVE: {
        float current = getRelativeAngle();
        float error = shortestError(targetAngle, current);

        if (abs(error) < ANGLE_TOL) {
            // Serial.println("Offset reached.");

            float raw = as5600.readAngle() * 360.0 / 4096.0;
            lastRaw = raw;
            continuousAngle = 0;
            zeroRef = 0;

            stepEnabled = false;
            state = WAITING;
            // motorActionTimer.setTimeout(2000, afterOffsetWait);
        }
        break;
        }

        case RUN:
        if (!holding && inPosition) {
            holding = true;
            stepEnabled = false;

            // Serial.println("Holding...");
            // motorActionTimer.setTimeout(3000, moveBackToHome);
        }
        break;

        case WAITING:
        break;
    }
}

void setupMotor()
{
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(HALL_PIN, INPUT_PULLUP);

    lastRaw = as5600.readAngle() * 360.0 / 4096.0;

    stepTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(stepTimer, &onStepTimer, true);
    timerAlarmWrite(stepTimer, 2000, true);
    timerAlarmEnable(stepTimer);

    Serial.println("Motor Initialized");
}

void updateMotor() {
    stepTask();
    motorActionTimer.run();
    motorState();
}