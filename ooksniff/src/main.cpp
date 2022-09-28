#include <Arduino.h>
#include <cppQueue.h>

#define QUEUE_LENGTH 400

cppQueue q(sizeof(int), QUEUE_LENGTH, FIFO);

volatile unsigned int newDuration;

void IRAM_ATTR handleInterrupt()
{
  static unsigned long lastTime = 0;

  const unsigned long time = micros();
  newDuration = time - lastTime;

  lastTime = time;
}

void enableReceive(int interrupt)
{
  attachInterrupt(interrupt, handleInterrupt, CHANGE);
}

void setup()
{
  Serial.begin(9600);
  newDuration = 0;
  enableReceive(D5);
}

void loop()
{
  if (newDuration != 0)
  {
    // newDuration is volatile so we don't want to pass that
    unsigned int duration = newDuration;
    q.push(&duration);

    newDuration = 0;
  }

  if (q.isFull())
  {
    for (int i = 0; i < QUEUE_LENGTH; i++)
    {
      int val;
      q.pop(&val);
      Serial.println(val);
    }
    Serial.println();
    Serial.println();
    Serial.println();
  }
}