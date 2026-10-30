#include "event_orchestrator.h"
#include "esp_err.h"
#include "esp_event.h"

#include "esp_event_base.h"

ESP_EVENT_DEFINE_BASE(APP_EVENT_ORCHESTRATOR);

static esp_event_loop_handle_t loop_handle;

esp_err_t event_orchestrator_init(void) {
  esp_event_loop_args_t loop_args = {
      .queue_size = 16,
      .task_name = "event_orchestrator_task",
      .task_priority = 5,
      .task_stack_size = 4096,
      .task_core_id = tskNO_AFFINITY,
  };

  esp_err_t err = esp_event_loop_create(&loop_args, &loop_handle);

  if (err != ESP_OK) {
    return err;
  }

  return err;
}

esp_event_loop_handle_t event_orchestrator_get_loop(void) {
  return loop_handle;
}

esp_err_t event_orchestrator_add_event_handler(
    esp_event_base_t event_base, int32_t event_id,
    esp_event_handler_t event_handler, void *event_handler_arg) {
  return esp_event_handler_instance_register_with(loop_handle, event_base,
                                                  event_id, event_handler,
                                                  event_handler_arg, NULL);
}
