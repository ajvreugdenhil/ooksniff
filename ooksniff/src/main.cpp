#include <Arduino.h>
#include <cppQueue.h>

#define QUEUE_LENGTH 512

#define MIN_CODE_LENGTH 8
#define MAX_CODE_LENGTH 34
#define TIMING_PRECISION_PERCENTAGE 10

cppQueue q(sizeof(int), QUEUE_LENGTH, FIFO, true);

volatile unsigned int newDuration;
volatile bool durationMissed;

void IRAM_ATTR handleInterrupt()
{
  static unsigned long lastTime = 0;

  if (newDuration != 0)
  {
    durationMissed = true;
  }

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
  Serial.begin(460800);
  newDuration = 0;
  durationMissed = false;
  enableReceive(D5);
}

bool validateQueue(cppQueue* q)
{
  if (q->getCount() <= 16)
  {
    return false;
  }

  for (int i = 8; i < q->getCount(); i++)
  {
    int matches = 0;
    for (int j = 0; j < 8; j++)
    {
      int x;
      q->peekIdx(&x, j);
      int y;
      q->peekIdx(&y, i);
      if (abs(x-y) < ((((x + y) / 2) * TIMING_PRECISION_PERCENTAGE) / 100))
      {
        matches++;
      }
    }
    if (matches >= 7)
    {
      Serial.println(i);
      return true;
    }
  }
  return false;
}

bool transmissionFinished(cppQueue* q)
{
  int durationCount = 4;
  unsigned int total = 0;

  if (q->getCount() < durationCount)
  {
    return false;
  }

  for (int i = 0; i < durationCount; i++)
  {
    int value;
    q->peekIdx(&value, i);
    //q->peekIdx(&value, (q->getCount()) - i);
    total += value;
  }
  unsigned int threshold = 2000;
  if ((total/durationCount) > threshold)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void loop()
{
  if (newDuration != 0)
  {
    // newDuration is volatile so we explicitly take its current value
    unsigned int duration = newDuration;
    q.push(&duration);
    newDuration = 0;

    // When we encounter a long duration between edges, we cannot be inside
    // an active transmission. So this is the best time to send saved data
    // to be analyzed.
    if (duration > 6000)
    {
      if ((q.getCount()>7) && validateQueue(&q))
      {
        Serial.println("Match found");
      }
      else
      {
        q.clean();
      }
      // validation takes long, we expect to miss signals during this time
      // so we override the warning
      durationMissed = false;
    }

    /*
    if (q.isFull())
    {
      while (!q.isEmpty())
      {
        int n;
        q.pop(&n);
        Serial.println(n);
      }
      Serial.println();
      Serial.println();
    }
    */
  }

  if (durationMissed)
  {
    Serial.println("!");
    durationMissed = false;
  }
}