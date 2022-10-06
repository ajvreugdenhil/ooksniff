#include <Arduino.h>
#include <cppQueue.h>

#define QUEUE_LENGTH 1024
#define TIMING_PRECISION_PERCENTAGE 50
#define SIGNAL_INPUT_PIN D5
#define SIGNAL_OUTPUT_PIN D4

cppQueue q(sizeof(int), QUEUE_LENGTH, FIFO, true);

volatile unsigned int newDuration;
volatile bool durationMissed;

struct dataFrame
{
  int baseDuration;
  int length;
  int* data;
};

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
  pinMode(SIGNAL_OUTPUT_PIN, OUTPUT);
  digitalWrite(SIGNAL_OUTPUT_PIN, LOW);
}

// Cannot handle much noise
// Relies on minimum frame size and minimum baud rate
bool validateQueueSimple(cppQueue *q)
{
  int count = q->getCount();
  int maxExpectedAverageDuration = 2000;
  int minDataLength = 16;

  if (count < minDataLength)
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

void printFrame(dataFrame* frame)
{
  Serial.print("(t:");
  Serial.print(frame->baseDuration);
  Serial.print(") ");
  for (int i = 0; i < frame->length; i++)
  {
    int x = frame->data[i];
    if (x == -1)
    {
      Serial.print("?");
    }
    else
    {
      Serial.print(x);
    }
    Serial.print(" ");
  }
  Serial.println();
}

bool populateFrame(cppQueue* q, dataFrame* frame)
{
  if (q == NULL || frame == NULL || frame->length == 0 || frame->data == NULL)
  {
    return false;
  }
  // Find lowest value in queue
  int queueCount = q->getCount();
  int lowest;

  q->peekIdx(&lowest, 0);
  for (int i = 0; i < queueCount; i++)
  {
    int x;
    q->peekIdx(&x, i);
    if (x < lowest)
    {
      lowest = x;
    }
  }

  // Find average of all values that are within 10% of lowest.
  // Median may be better but is more expensive
  unsigned int total = 0;
  int count = 0;
  for (int i = 0; i < queueCount; i++)
  {
    int x;
    q->peekIdx(&x, i);
    if (abs(lowest - x) < ((lowest * TIMING_PRECISION_PERCENTAGE) / 100))
    {
      count++;
      total += x;
    }
  }
  frame->baseDuration = total / count;

  for (int i = 0; i < queueCount; i++)
  {
    if (i >= frame->length)
    {
      return false;
    }
    int x;
    q->peekIdx(&x, i);
    bool found = false;
    for (int j = 0; j < 12; j++)
    {
      if ((x > ((j * frame->baseDuration * (100 - TIMING_PRECISION_PERCENTAGE) / 100))) && (x < ((j * frame->baseDuration * (100 + TIMING_PRECISION_PERCENTAGE) / 100))))
      {
        frame->data[i] = j;
        found = true;
        break;
      }
    }
    if (!found)
    {
      frame->data[i] = -1;
    }
  }
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
          int count = q.getCount();
          int data[count];
          dataFrame frame = {0, count, data};
          populateFrame(&q, &frame);
          printFrame(&frame);
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