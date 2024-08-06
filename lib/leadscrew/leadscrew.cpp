#include "leadscrew.h"

#include <globalstate.h>

#include <cstdint>

#include "leadscrew_io.h"

Leadscrew::Leadscrew(Axis* leadAxis, LeadscrewIO* io) {
  m_leadAxis = leadAxis;
  m_io = io;
  m_ratio = 1.0;
  Axis::m_currentPosition = 0;
  m_lastPulseMicros = 0;
  m_currentPulseDelay = LEADSCREW_INITIAL_PULSE_DELAY_US;
  m_accumulator = 0;
}

void Leadscrew::setRatio(float ratio) {
  m_ratio = ratio;
  // extrapolate the current position based on the new ratio
  m_currentPosition = m_leadAxis->getCurrentPosition() * m_ratio;
}

float Leadscrew::getRatio() { return m_ratio; }

int Leadscrew::getExpectedPosition() {
  return m_leadAxis->getCurrentPosition() * m_ratio;
}

int Leadscrew::getCurrentPosition() { return m_currentPosition; }

void Leadscrew::resetCurrentPosition() {
  m_currentPosition = m_leadAxis->getCurrentPosition() * m_ratio;
}

void Leadscrew::setStopPosition(StopPosition position, int stopPosition) {
  switch (position) {
    case LEFT:
      m_leftStopPosition = stopPosition;
      break;
    case RIGHT:
      m_rightStopPosition = stopPosition;
      break;
  }
}

int Leadscrew::getStopPosition(StopPosition position) {
  switch (position) {
    case LEFT:
      return m_leftStopPosition;
    case RIGHT:
      return m_rightStopPosition;
  }
  return 0;
}

void Leadscrew::setCurrentPosition(int position) {
  m_currentPosition = position;
}

void Leadscrew::incrementCurrentPosition(int amount) {
  m_currentPosition += amount;
}

float Leadscrew::getAccumulatorUnit() {
  return (ELS_LEADSCREW_STEPS_PER_MM * getRatio()) / ELS_LEADSCREW_STEPPER_PPR;
}

bool Leadscrew::sendPulse() {
  uint8_t pinState = m_io->readStepPin();

  // Keep the pulse pin high as long as we're not scheduled to send a pulse
  if (pinState == 1 && m_lastPulseMicros > m_currentPulseDelay) {
    m_io->writeStepPin(0);
    m_lastFullPulseDurationMicros = m_lastPulseMicros;
    m_lastPulseMicros = 0;

    if (m_accumulator > 1) {
      m_accumulator--;
    }
    if (m_accumulator < -1) {
      m_accumulator++;
    }
  } else {
    m_io->writeStepPin(1);
  }

  return pinState == 1;
}

void Leadscrew::update() {
  GlobalState* globalState = GlobalState::getInstance();

  int positionError = getPositionError();

  int directionIncrement = 0;

  if (positionError > 0) {
    m_io->writeDirPin(1);
    directionIncrement = 1;
  } else if (positionError < 0) {
    m_io->writeDirPin(0);
    directionIncrement = -1;
  }

  switch (globalState->getMotionMode()) {
    case GlobalMotionMode::DISABLED:
      // ignore the spindle, pretend we're in sync all the time
      resetCurrentPosition();
      break;
    case GlobalMotionMode::JOG:
      // only send a pulse if we haven't sent one recently
      if (m_lastPulseMicros < JOG_PULSE_DELAY_US) {
        break;
      }
      // if jog is complete go back to disabled motion mode
      if (positionError == 0) {
        globalState->setMotionMode(GlobalMotionMode::DISABLED);
      }
      // jogging is a fixed rate based on JOG_PULSE_DELAY_US
      sendPulse();

      break;
    case GlobalMotionMode::ENABLED:

      if (positionError == 0) {
        // todo check if we have sync'd before
        globalState->setThreadSyncState(GlobalThreadSyncState::SYNC);
        break;
      }

      // attempt to keep in sync with the leadscrew
      // if sendPulse returns true, we've actually sent a pulse
      if (sendPulse()) {
        m_currentPosition += directionIncrement;

        // calculate the stopping distance based on current speed and set accel
        int stoppingDistanceInPulses =
            (LEADSCREW_INITIAL_PULSE_DELAY_US - m_currentPulseDelay) /
            LEADSCREW_PULSE_DELAY_STEP_US / LEADSCREW_TIMER_US;

        // if this is true we should start decelerating to stop at the correct
        // position
        bool shouldStop = directionIncrement > 0
                              ? m_currentPosition + stoppingDistanceInPulses >=
                                    m_rightStopPosition
                              : m_currentPosition - stoppingDistanceInPulses <=
                                    m_leftStopPosition;

        if (shouldStop) {
          // if we're close to the stopping position we should start
          // decelerating
          m_currentPulseDelay += LEADSCREW_PULSE_DELAY_STEP_US;
          // todo last pulse duration variable should be a getter instead
        } else if (getPositionError() > 0) {
          m_currentPulseDelay -= LEADSCREW_PULSE_DELAY_STEP_US;
        }

        // if pulse is sent we want to calculate how much to change the timing
        // for the next pulse
        // depending on accel and current speed etc
        // inital pulse delay is upper timing limit
        //
        if (m_currentPulseDelay > LEADSCREW_INITIAL_PULSE_DELAY_US) {
          m_currentPulseDelay = LEADSCREW_INITIAL_PULSE_DELAY_US;
        }
        if (m_currentPulseDelay < 0) {
          m_currentPulseDelay = 0;
        }
      }

      if ((int)(m_accumulator) != 0) {
        m_accumulator += getAccumulatorUnit();
      }

      m_lastFullPulseDurationMicros = m_lastPulseMicros;
      m_lastPulseMicros = 0;

      break;
  }
}

int Leadscrew::getPositionError() {
  return getExpectedPosition() - getCurrentPosition();
}

float Leadscrew::getEstimatedVelocityInMillimetersPerSecond() {
  return getEstimatedVelocityInPulsesPerSecond() / ELS_LEADSCREW_STEPS_PER_MM;
}
