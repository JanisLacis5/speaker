#include "task_handler.hpp"

void task_handler::init() {
    task_queue_ = xQueueCreate(10, sizeof(task*));
    xTaskCreate(main_task, "BtAppTask", 3072, &task_queue_, 10, &task_handle_);
}

task_handler::~task_handler() {
    vTaskDelete(task_handle_);
}

void task_handler::main_task(void* arg) {
    auto* task_queue = static_cast<QueueHandle_t*>(arg);
    task* task = nullptr;

    ESP_LOGI(TH_TAG, "Supported signals:");
    ESP_LOGI(TH_TAG, "BT_APP_SIG_DISPATCH: %d", app_signal::BT_APP_SIG_WORK_DISPATCH);
    ESP_LOGI(TH_TAG, "BT_APP_SIG_BAD: %d", app_signal::BT_APP_SIG_BAD);

    for (;;) {
        if (xQueueReceive(*task_queue, &task, (TickType_t)portMAX_DELAY) == pdTRUE) {
            if (!task) {
                ESP_LOGE(TH_TAG, "task retrieval succeeded but it is null");
                continue;
            }

            switch (task->signal) {
            case app_signal::BT_APP_SIG_WORK_DISPATCH:
                task->execute();
                break;
            default:
                ESP_LOGW(TH_TAG, "%s, unhandled signal: %d", __func__, task->signal);
                break;
            }
        }

        if (task)
            delete task;
    }
}

bool task_handler::add_task(const task* task) {
    if (xQueueSend(task_queue_, &task, 10 / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE(TH_TAG, "%s xQueue send failed", __func__);
        return false;
    }

    return true;
}
