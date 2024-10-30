/*
VSPI is SPI 3

SPID = MOSI = data out
SPIQ = MISO = data in

VSPICS0   GPIO 5
VSPICLK   GPIO 18
VSPID     GPIO 23  MOSI
VSPIQ     GPIO 19  MISO

TODO: the two interrupt pins
*/

#include <Arduino.h>
#include <cppQueue.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

//#define QUEUE_LENGTH 1024
#define QUEUE_LENGTH 512
#define TIMING_PRECISION_PERCENTAGE 50
#define GDO0_PIN 17
#define GDO2_PIN 16
#define LED_PIN 22

#define DURATION_FILTER_MIN 50
#define DURATION_FILTER_MAX 5000
#define FILTER_DATA_COUNT_MIN 16

cppQueue* queue = NULL;

TaskHandle_t analyze_task_handle;
bool analysis_active = false;

volatile unsigned int newDuration;
volatile bool durationMissed;
volatile bool transmissionFinished;

struct dataFrame
{
  int baseDuration;
  int length;
  int* data;
};

void IRAM_ATTR interruptGDO0()
{
  static unsigned long lastTime = 0;
  if (newDuration != 0)
  {
    durationMissed = true;
    digitalWrite(LED_PIN, LOW);
  }
  const unsigned long time = micros();
  newDuration = time - lastTime;
  lastTime = time;
}

void IRAM_ATTR interruptGDO2()
{
  transmissionFinished = true;
}

void enableReceive(int pin)
{
  attachInterrupt(digitalPinToInterrupt(pin), interruptGDO0, CHANGE);
}



void setup()
{
  Serial.begin(460800);
  newDuration = 0;
  durationMissed = false;
  pinMode(GDO0_PIN, INPUT);
  pinMode(GDO2_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  queue = new cppQueue(sizeof(int), QUEUE_LENGTH, FIFO, true);
  enableReceive(GDO0_PIN);
  attachInterrupt(digitalPinToInterrupt(GDO2_PIN), interruptGDO2, FALLING);

  Serial.println("Starting cc1101");
  if (ELECHOUSE_cc1101.getCC1101())
  {
    Serial.println("Connection OK");
  }
  else
  {
    Serial.println("Connection Error");
  }
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setGDO0(GDO0_PIN);
  ELECHOUSE_cc1101.setCCMode(0);
  ELECHOUSE_cc1101.setModulation(2);  // 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.setSyncMode(0);    // Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.
  ELECHOUSE_cc1101.setCrc(0);
  ELECHOUSE_cc1101.setPktFormat(3);   // Asynchronous mode
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG0, 0x0D);          // Set GDO0 to serial data output (0x0D)
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG2, 0x0E);          // Set GDO2 to carrier sense
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01110111);  // set carrier sense thresholds
  ELECHOUSE_cc1101.SetRx();
  Serial.println("Started cc1101");
}

// Cannot handle much noise
// Relies on minimum frame size and minimum baud rate
bool validateQueueSimple(cppQueue *q)
{
  int count = q->getCount();
  int maxExpectedAverageDuration = 2000;

  if (count < FILTER_DATA_COUNT_MIN)
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
    if (x < lowest && x > DURATION_FILTER_MIN)
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

void Analyze(void * parameter)
{
  cppQueue* q_analysis = (cppQueue*) parameter;
  if (!durationMissed)
  {
    if (validateQueueSimple(q_analysis))
    {
      int count = q_analysis->getCount();
      int data[count];
      dataFrame frame = {0, count, data};
      populateFrame(q_analysis, &frame);
      printFrame(&frame);
    }
  }
  delete(q_analysis);
  analysis_active = false;
  vTaskDelete(analyze_task_handle);
}

void loop()
{
  if (newDuration != 0)
  {
    // newDuration is volatile so we explicitly take its current value
    unsigned int duration = newDuration;
    queue->push(&duration);
    newDuration = 0;

    // When we encounter a long duration between edges, we cannot be inside
    // an active transmission. So this is the best time to send saved data
    // to be analyzed.
    //if (duration > DURATION_FILTER_MAX && !analysis_active)
    
    if (transmissionFinished && !analysis_active)
    {
      if (queue->getCount() > FILTER_DATA_COUNT_MIN)
      {
        cppQueue* q_analysis = queue;
        queue = new cppQueue(sizeof(int), QUEUE_LENGTH, FIFO, true);

        xTaskCreatePinnedToCore(
          Analyze,    /* Function to implement the task */
          "analyze",  /* Name of the task */
          10000,      /* Stack size in words */
          q_analysis, /* Task input parameter */
          0,          /* Priority of the task */
          &analyze_task_handle,  /* Task handle. */
          0);         /* Core where the task should run */
        analysis_active = true;
      }
      else
      {
        queue->clean();
      }
      transmissionFinished = false;
    }
  }

  if (durationMissed)
  {
    //Serial.println("!");
    durationMissed = false;
    digitalWrite(LED_PIN, HIGH);
  }
}