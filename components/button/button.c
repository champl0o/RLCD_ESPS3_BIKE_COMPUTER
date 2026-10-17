#include "button.h"
#include "gesture.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

#define BOOT_PIN 0
#define KEY_PIN 18

#define DEBOUNCE_TIME_US (30 * 1000)

static QueueHandle_t button_queue;
static QueueHandle_t event_queue;
static const char *TAG = "button";

static btn_state_t button_states[BTN_COUNT] =
    {
        {.state = IDLE,
         .debounce_deadline_us = NO_DEADLINE,
         .gesture_deadline_us = NO_DEADLINE,
         .btn_id = BTN_BOOT},
        {
            .state = IDLE,
            .debounce_deadline_us = NO_DEADLINE,
            .gesture_deadline_us = NO_DEADLINE,
            .btn_id = BTN_KEY,
        }};

char *gesture_to_string(btn_gesture_t gesture)
{
    return gesture == GESTURE_NONE ? "none" : gesture == PRESS_SHORT ? "press_short"
                                          : gesture == PRESS_LONG    ? "press_long"
                                          : gesture == PRESS_DOUBLE  ? "press_double"
                                                                     : "unknown";
}

static void emit_gesture(btn_id_t id, btn_gesture_t gesture)
{
    if (gesture != GESTURE_NONE)
    {
        ESP_LOGI(TAG, "Gesture: %s", gesture_to_string(gesture));

        btn_event_t event = {
            .btn_id = id,
            .gesture = gesture};
        xQueueSend(event_queue, &event, 0);
    }
}

static btn_gesture_t handle_debounce(btn_state_t *b, int64_t now)
{
    bool deadline_passed = (b->debounce_deadline_us != NO_DEADLINE && b->debounce_deadline_us <= now);
    if (!deadline_passed)
        return GESTURE_NONE;

    b->debounce_deadline_us = NO_DEADLINE;

    bool pressed = gpio_get_level(b->btn_id == BTN_BOOT ? BOOT_PIN : KEY_PIN) == 0;
    if (pressed != b->stable)
    {
        b->stable = pressed;

        btn_gesture_t gesture = handle_event(b, pressed ? PRESS : RELEASE, now);
        ESP_LOGI(TAG, "Button %d %s", b->btn_id, pressed ? "pressed" : "released");
        return gesture;
    }
    return GESTURE_NONE;
}

static btn_gesture_t handle_gesture_deadline(btn_state_t *b, int64_t now)
{
    bool gesture_deadline_passed = (b->gesture_deadline_us != NO_DEADLINE && b->gesture_deadline_us <= now);
    if (gesture_deadline_passed)
    {
        return handle_event(b, TIMEOUT, now);
    }
    return GESTURE_NONE;
}

static void handle_button(btn_id_t id, int64_t now)
{
    btn_state_t *b = &button_states[id];

    emit_gesture(id, handle_debounce(b, now));

    emit_gesture(id, handle_gesture_deadline(b, now));
}

static TickType_t timeout_ticks(int64_t now)
{
    int64_t next = NO_DEADLINE;
    for (int i = 0; i < BTN_COUNT; i++)
    {
        int64_t d = button_states[i].debounce_deadline_us;
        int64_t g = button_states[i].gesture_deadline_us;

        if (g != NO_DEADLINE && (next == NO_DEADLINE || g < next))
            next = g;
        if (d != NO_DEADLINE && (next == NO_DEADLINE || d < next))
            next = d;
    }

    if (next == NO_DEADLINE)
        return portMAX_DELAY;
    if (next <= now)
        return 0;
    return pdMS_TO_TICKS((next - now + 999) / 1000) + 1;
}

static void button_task(void *arg)
{
    ESP_LOGI(TAG, "Button task started");
    while (1)
    {
        btn_id_t id;
        bool received = xQueueReceive(button_queue, &id, timeout_ticks(esp_timer_get_time()));
        int64_t now = esp_timer_get_time();

        if (received)
        {
            button_states[id].debounce_deadline_us = now + DEBOUNCE_TIME_US;
        }

        for (int i = 0; i < BTN_COUNT; i++)
        {
            handle_button((btn_id_t)i, now);
        }
    }
}

static void IRAM_ATTR on_edge(void *arg)
{
    btn_id_t id = (btn_id_t)(uintptr_t)arg;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(button_queue, &id, &woken);
    portYIELD_FROM_ISR(woken);
}

void button_init(void)
{
    button_queue = xQueueCreate(16, sizeof(btn_id_t));
    event_queue = xQueueCreate(16, sizeof(btn_event_t));

    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BOOT_PIN) | (1ULL << KEY_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOOT_PIN, on_edge, (void *)(uintptr_t)BTN_BOOT);
    gpio_isr_handler_add(KEY_PIN, on_edge, (void *)(uintptr_t)BTN_KEY);

    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);
}
