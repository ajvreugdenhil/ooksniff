#include <Arduino.h>
#include <cppQueue.h>

#define QUEUE_LENGTH 1024
#define TIMING_PRECISION_PERCENTAGE 50
#define SIGNAL_INPUT_PIN D5

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
  attachInterrupt(digitalPinToInterrupt(SIGNAL_INPUT_PIN), handleInterrupt, CHANGE);
}

void setup()
{
  Serial.begin(460800);
  newDuration = 0;
  durationMissed = false;
  pinMode(SIGNAL_INPUT_PIN, INPUT);
  enableReceive(SIGNAL_INPUT_PIN);
}

// Relies on the data being repeatedly sent in one transmission
bool validateQueueRepeatingPattern(cppQueue *q)
{
  int durationsToVerifyCount = 8;

  if (q->getCount() <= durationsToVerifyCount * 2)
  {
    return false;
  }

  for (int i = durationsToVerifyCount; i < q->getCount(); i++)
  {
    int matches = 0;
    for (int j = 4; j < durationsToVerifyCount + 4; j++)
    {
      int x;
      q->peekIdx(&x, j);
      int y;
      q->peekIdx(&y, i + j);
      if (abs(x - y) < ((((x + y) / 2) * TIMING_PRECISION_PERCENTAGE) / 100))
      {
        matches++;
      }
    }
    if (matches == durationsToVerifyCount)
    {
      return true;
    }
  }
  return false;
}

// Cannot handle much noise
bool validateQueueSimple(cppQueue *q)
{
  int count = q->getCount();
  int maxExpectedAverageDuration = 2000;
  int minDataLength = 8;

  if (count < minDataLength*2)
  {
    return false;
  }

  int total = 0;
  for (int i = 0; i < count; i++)
  {
    int x;
    q->peekIdx(&x, i);
    total += x;
  }
  int average = total/count;
  return (average < maxExpectedAverageDuration);
}

void printQueueRaw(cppQueue *q)
{
  while (!q->isEmpty())
  {
    int n;
    q->pop(&n);
    Serial.println(n);
  }
  Serial.println();
  Serial.println();
}

bool printQueueSimplified(cppQueue *q)
{
  int lowest;
  q->peekIdx(&lowest, 0);
  for (int i = 0; i < q->getCount(); i++)
  {
    int x;
    q->peekIdx(&x, i);
    if (x < lowest)
    {
      lowest = x;
    }
  }

  unsigned int total = 0;
  int count = 0;
  for (int i = 0; i < q->getCount(); i++)
  {
    int x;
    q->peekIdx(&x, i);
    if (abs(lowest - x) < ((lowest * TIMING_PRECISION_PERCENTAGE) / 100))
    {
      count++;
      total += x;
    }
  }
  int baseDuration = total / count;

  Serial.print("(t:");
  Serial.print(baseDuration);
  Serial.print(") ");
  Serial.print("(c:");
  Serial.print(q->getCount());
  Serial.print(") ");
  for (int i = 0; i < q->getCount(); i++)
  {
    int x;
    q->pop(&x);
    bool found = false;
    for (int j = 0; j < 12; j++)
    {
      if ((x > ((j * baseDuration * (100 - TIMING_PRECISION_PERCENTAGE) / 100))) && (x < ((j * baseDuration * (100 + TIMING_PRECISION_PERCENTAGE) / 100))))
      {
        Serial.print(j);
        found = true;
        break;
      }
    }
    if (!found)
    {
      Serial.print("?");
    }
    Serial.print(" ");
  }
  Serial.println();
  return true;
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
    if (duration > 8000)
    {
      if (!durationMissed)
      {
        if (validateQueueSimple(&q))
        {
          printQueueSimplified(&q);
          // printQueueRaw(&q);
          //  Full validation can take long, we expect to miss signals during this time
          //  so we override the warning
          durationMissed = false;
        }
      }
      q.clean();
    }
  }

  if (durationMissed)
  {
    Serial.println("!");
    durationMissed = false;
  }
}