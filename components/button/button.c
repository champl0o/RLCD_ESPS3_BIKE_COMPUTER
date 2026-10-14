#include "button.h"

static QueueHandle_t button_queue;
static const char *TAG = "button";
#define NO_DEADLINE (-1)

static btn_state_t button_states[BTN_COUNT] =
    {{.state = IDLE, .deadline_us = NO_DEADLINE},
     {.state = IDLE, .deadline_us = NO_DEADLINE}};

static void handle_button(btn_id_t id, int64_t now)
{
    btn_state_t *b = &button_states[id];

    if (b->deadline_us == NO_DEADLINE || b->deadline_us > now)
        return;

    // Дедлайн минув: брязкіт скінчився, скидаємо його завжди,
    // незалежно від того, чи змінився стан.
    b->deadline_us = NO_DEADLINE;

    bool pressed = gpio_get_level(id == BTN_BOOT ? BOOT_PIN : KEY_PIN) == 0;
    if (pressed != b->stable)
    {
        b->stable = pressed;
        ESP_LOGI(TAG, "Button %d %s", id, pressed ? "pressed" : "released");
    }
}

// Скільки тіків чекати до найближчого дедлайну серед усіх кнопок.
// Округляємо вгору + 1 тік, щоб після пробудження гарантовано now >= deadline.
static TickType_t timeout_ticks(int64_t now)
{
    int64_t next = NO_DEADLINE;
    for (int i = 0; i < BTN_COUNT; i++)
    {
        int64_t d = button_states[i].deadline_us;
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
            button_states[id].deadline_us = now + DEBOUNCE_TIME_US;
        }
        else
        {
            for (int i = 0; i < BTN_COUNT; i++)
            {
                handle_button((btn_id_t)i, now);
            }
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
