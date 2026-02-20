#pragma once

#include <Arduino.h>

class StateMachine {
public:
  enum class State {
    IDLE,
    SCANNING,
    READY,
    ERROR
  };
  
  static void init();
  static void update();
  static State getState() { return currentState; }
  static void setState(State state);
  static const char* getStateString();
  
  // Job management
  static void startScan();
  static const char* createJob(const char* action, const char* params);
  static void completeJob(const char* jobId, bool success);
  static const char* getCurrentJobId() { return currentJobId; }
  
private:
  static State currentState;
  static char currentJobId[64]; // Fixed buffer instead of String
  static unsigned long jobCounter;
  
  static void generateJobId(char* buffer, size_t bufferSize);
};

