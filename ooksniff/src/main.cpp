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


String shrew = "          _,____ c--"
   ".\r\n"
   "        /`  \\   ` _"
   "^_\\\r\n"
   " jgs`~~~\\  _/---\'"
   "\\\\  ^\r\n"
   "         `~~~     ~~"
   " ";

#define QUEUE_LENGTH 1024
#define GDO0_PIN 17
#define GDO2_PIN 16
#define LED_PIN 22

#define TIMING_PRECISION_PERCENTAGE 50
#define DURATION_FILTER_MIN 10
#define DURATION_FILTER_MAX 5000
#define FILTER_DATA_COUNT_MIN 16

cppQueue* queue = NULL;

TaskHandle_t analyze_task_handle;
bool analysis_active = false;

volatile unsigned int newDuration;
volatile bool durationMissed;
volatile unsigned int lastTrasmissionTime;
volatile bool transmissionFinshed = false;

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
    //digitalWrite(LED_PIN, LOW);
  }
  const unsigned long time = micros();
  newDuration = time - lastTime;
  lastTime = time;
}

void IRAM_ATTR interruptGDO2()
{
  lastTrasmissionTime = micros();
  transmissionFinshed = true;
}

void enableReceive(int pin)
{
  attachInterrupt(digitalPinToInterrupt(pin), interruptGDO0, CHANGE);
}



void setup()
{
  Serial.begin(460800);
  delay(1000);
  Serial.println("What the sneef?");
  delay(1000);
  Serial.println(shrew);
  delay(1000);
  Serial.println("I'm snorfin' here!!!\n\n");
  delay(3000);


  newDuration = 0;
  durationMissed = false;
  lastTrasmissionTime = 0;
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

//   The absolute threshold related to the RSSI
// value depends on the following register fields:
// AGCCTRL2.MAX_LNA_GAIN
// AGCCTRL2.MAX_DVGA_GAIN
// AGCCTRL1.CARRIER_SENSE_ABS_THR
// AGCCTRL2.MAGN_TARGET

//If the threshold is set high, i.e. only strong
// signals are wanted, the threshold should be
// adjusted upwards by first reducing the
// MAX_LNA_GAIN value and then the
// MAX_DVGA_GAIN value.

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
  
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, 0b00000011);  // set gain limits (reset value)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, 0b00111011);  // set gain limits (maximally limited lna gain)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, 0b11111011);  // set gain limits (maximally limited lna and dvga gain)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, 0b11000111);  // set gain limits (lib default)
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, 0b11100111);  // set gain limits (mine)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, );  // set gain limits (test)
  

  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01000000);  // set carrier sense thresholds (reset value)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01001000);  // set carrier sense thresholds (no relative, no abs)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01001001);  // set carrier sense thresholds (no relative, -7 abs)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01000111);  // set carrier sense thresholds (no rel, +7 abs)
  // ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01111000);  // set carrier sense thresholds (hi rel, no abs)
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01000111);  // set carrier sense thresholds ()
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0b01110111);  // set carrier sense thresholds ()
  
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
    if (x < lowest) // && x > DURATION_FILTER_MIN) // ignore very short times
    {
      lowest = x;
    }
  }

  // Find average of all values that are within n% of lowest.
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
      if (frame.baseDuration <= DURATION_FILTER_MAX)
      {
        printFrame(&frame);
      }
    }
  }
  delete(q_analysis);
  analysis_active = false;
  vTaskDelete(analyze_task_handle);
}

void loop()
{
  digitalWrite(LED_PIN, !digitalRead(GDO0_PIN));


  if (newDuration > 100)
  {
    // newDuration is volatile so we explicitly take its current value
    unsigned int duration = newDuration;
    queue->push(&duration);
  }
  newDuration = 0;
  

  if (!analysis_active)
  {
    if (transmissionFinshed && queue->getCount() > FILTER_DATA_COUNT_MIN)
    {
      transmissionFinshed = false;
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
    
      queue->clean();
    }
  }


  if (durationMissed)
  {
    //Serial.println("!");
    durationMissed = false;
    //digitalWrite(LED_PIN, HIGH);
  }
}