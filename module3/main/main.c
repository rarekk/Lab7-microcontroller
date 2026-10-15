#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define BUTTON_PIN         GPIO_NUM_4
#define DEBOUNCE_TIME_US   50000   // 50 ms debounce window

static const char *TAG = "RTOS_ISR_PROJECT";
static QueueHandle_t gpio_evt_queue = NULL;

// 1. Interrupt Service Routine placed in internal Instruction RAM (IRAM).
//    Keep it minimal: never debounce or log here, just offload to the queue.
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// 2. Background Worker Consumer Thread
static void button_worker_task(void *arg)
{
    uint32_t io_num;
    uint32_t press_count = 0;
    int64_t last_press_us = 0;

    while (1) {
        // Block indefinitely until an ISR event is pushed to the queue
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            int64_t now_us = esp_timer_get_time();

            // Contact bounce: a real press produces many edges within ~1 ms.
            // Ignore any edge that arrives too soon after the previous one,
            // so one physical click counts exactly once.
            if (now_us - last_press_us < DEBOUNCE_TIME_US) {
                continue;
            }
            last_press_us = now_us;

            press_count++;
            ESP_LOGI(TAG, "Button pressed! Count = %" PRIu32 " (GPIO %" PRIu32 ")",
                     press_count, io_num);
        }
    }
}

void app_main(void)
{
    // 3. Create FreeRTOS Queue for Event Offloading
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 4. Spawn Consumer Task
    xTaskCreate(button_worker_task, "button_worker_task", 3072, NULL, 10, NULL);

    // 5. Configure Hardware Edge Trigger on Pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    // 6. Install Global ISR Service and Register Handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void *)(uintptr_t)BUTTON_PIN);

    ESP_LOGI(TAG, "GPIO Interrupt Service Routine installed. Waiting for hardware button presses...");
}
