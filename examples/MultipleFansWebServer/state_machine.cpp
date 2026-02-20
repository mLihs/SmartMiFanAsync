#include "state_machine.h"
#include <stdio.h>
#include <string.h>

StateMachine::State StateMachine::currentState = StateMachine::State::IDLE;
char StateMachine::currentJobId[64] = "";
unsigned long StateMachine::jobCounter = 0;

void StateMachine::init() {
  currentState = State::IDLE;
  currentJobId[0] = '\0';
  jobCounter = 0;
}

void StateMachine::update() {
  // State machine update logic
  // Can be extended for state transitions, timeouts, etc.
}

void StateMachine::setState(State state) {
  currentState = state;
}

const char* StateMachine::getStateString() {
  switch (currentState) {
    case State::IDLE: return "IDLE";
    case State::SCANNING: return "SCANNING";
    case State::READY: return "READY";
    case State::ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

void StateMachine::startScan() {
  currentState = State::SCANNING;
  generateJobId(currentJobId, sizeof(currentJobId));
}

const char* StateMachine::createJob(const char* action, const char* params) {
  generateJobId(currentJobId, sizeof(currentJobId));
  return currentJobId;
}

void StateMachine::completeJob(const char* jobId, bool success) {
  if (jobId != nullptr && strcmp(jobId, currentJobId) == 0) {
    currentJobId[0] = '\0';
  }
}

void StateMachine::generateJobId(char* buffer, size_t bufferSize) {
  jobCounter++;
  snprintf(buffer, bufferSize, "job_%lu_%lu", millis(), jobCounter);
}

