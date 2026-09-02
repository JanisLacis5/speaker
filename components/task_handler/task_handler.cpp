#include "task_handler.hpp"

void task_handler::init() {
    task_queue_ = xQueueCreate(10, sizeof(task*));
    xTaskCreate(main_task, "BtAppTask", 3072, this, 10, &task_handle_);
}

task_handler::~task_handler() {
    vTaskDelete(task_handle_);
}

void task_handler::main_task(void* arg) {
    auto* self = static_cast<task_handler*>(arg);
    task* task = nullptr;

    ESP_LOGI(TH_TAG, "Supported signals:");
    ESP_LOGI(TH_TAG, "BT_APP_SIG_DISPATCH: %d", app_signal::BT_APP_SIG_WORK_DISPATCH);
    ESP_LOGI(TH_TAG, "BT_APP_SIG_BAD: %d", app_signal::BT_APP_SIG_BAD);

    for (;;) {
        if (xQueueReceive(self->task_queue_, &task, (TickType_t)portMAX_DELAY) == pdTRUE) {
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

            self->remove_task(task);
        }
    }
}

void task_handler::remove_task(const task* task) {
    auto free_slot_iter = std::find_if(task_slots.begin(), task_slots.end(), [&task](auto& slot) { return slot.ptr == task; });
    if (free_slot_iter == task_slots.end())
        return;

    free_slot_iter->occupied = false;
    free_slot_iter->ptr->~task();
    free_slot_iter->ptr = nullptr;
}
