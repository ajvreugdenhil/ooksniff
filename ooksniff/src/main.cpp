#include <Arduino.h>
#include <cppQueue.h>

#define QUEUE_LENGTH 512

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
  int durationsToVerifyCount = 16;

  if (q->getCount() <= durationsToVerifyCount*2)
  {
    return false;
  }

  for (int i = durationsToVerifyCount; i < q->getCount(); i++)
  {
    int matches = 0;
    for (int j = 4; j < durationsToVerifyCount+4; j++)
    {
      int x;
      q->peekIdx(&x, j);
      int y;
      q->peekIdx(&y, i+j);
      if (abs(x-y) < ((((x + y) / 2) * TIMING_PRECISION_PERCENTAGE) / 100))
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

void printQueueRaw(cppQueue* q)
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

bool printQueueSimplified(cppQueue* q)
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
    if (abs(lowest-x) < ((lowest * TIMING_PRECISION_PERCENTAGE) / 100))
    {
      count++;
      total += x;
    }
  }
  int baseDuration = total/count;

  Serial.print("(");
  Serial.print(baseDuration);
  Serial.print(") ");
  for (int i = 0; i < q->getCount(); i++)
  {
    int x;
    q->pop(&x);
    bool found = false;
    for (int j = 0; j < 32; j++)
    {
      if ((x > ((j*baseDuration*(100-TIMING_PRECISION_PERCENTAGE)/100))) && (x < ((j*baseDuration*(100+TIMING_PRECISION_PERCENTAGE)/100))))
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
    if (duration > 6000)
    {
      if (validateQueue(&q))
      {
        printQueueSimplified(&q);
        // Full validation can take long, we expect to miss signals during this time
        // so we override the warning
        durationMissed = false;
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