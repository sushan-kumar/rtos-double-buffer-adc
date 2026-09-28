#include <Arduino_FreeRTOS.h>
#include "FspTimer.h"

FspTimer hardware_timer;

SemaphoreHandle_t xMutex = NULL;

float global_avg = 0.0;

const int ldr_pin = A0;

int bufferA[10];
int bufferB[10];

int * volatile active_buffer = bufferA;
volatile int buffer_index = 0;

QueueHandle_t xqueue = NULL;

void readData(timer_callback_args_t * p_args __attribute__((unused))){
  active_buffer[buffer_index] = analogRead(ldr_pin);
  buffer_index++;

  if (buffer_index == 10) {
    buffer_index = 0;

    int *completed_buffer = active_buffer;

    active_buffer = (active_buffer == bufferA) ? bufferB : bufferA;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(xqueue, &completed_buffer, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }

}

void TaskA_ProcessData(void *pvParameters){
  int *buf_to_process = NULL;

  while (1) {
    if (xQueueReceive(xqueue, &buf_to_process, portMAX_DELAY) == pdTRUE) {
      long sum = 0;
      for (int i =0; i < 10; i++) {
        sum += buf_to_process[i];
      }
      float calculated_avg = (float)sum/10.0;

      if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
        global_avg = calculated_avg;
        xSemaphoreGive(xMutex);
      }
    }
  }
}

void TaskB_SerialHandler(void *pvParameters){
  String input_cmd = "";

  while (1) {
    while (Serial.available()>0) {
      char c = Serial.read();
      Serial.print(c);

      if ( c == '\n' || c == '\r') {
        input_cmd.trim();
        if (input_cmd.equalsIgnoreCase("avg")) {
          float current_avg = 0.0;

          if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
            current_avg = global_avg;
            xSemaphoreGive(xMutex);
          }
          Serial.print("\nLDR average is: ");
          Serial.println(current_avg);
        }
        input_cmd = "";
      } else {
        input_cmd += c;
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000);
  Serial.println("Welocome to FreeRTOS Serial buffer");
  Serial.println("Type avg to get the avergae of the ldr value's\n");

  xqueue = xQueueCreate(2, sizeof(int *));
  xMutex = xSemaphoreCreateMutex();

  xTaskCreate(TaskA_ProcessData, "Task-A", 256, NULL, 2, NULL);
  xTaskCreate(TaskB_SerialHandler, "Task-B", 256, NULL, 1, NULL);

  uint8_t timer_type = GPT_TIMER;
  int8_t t_index = FspTimer::get_available_timer(timer_type);

  if (t_index < 0) { 
    t_index = FspTimer::get_available_timer(timer_type, true); 
    FspTimer::force_use_of_pwm_reserved_timer(); 
  }

  if (t_index >= 0) {
    hardware_timer.begin(TIMER_MODE_PERIODIC,timer_type,t_index,10.0,50.0,readData,nullptr);
    hardware_timer.setup_overflow_irq();
    hardware_timer.open();
    hardware_timer.start();
  }

  vTaskStartScheduler();
}

void loop() {
  // put your main code here, to run repeatedly:

}
