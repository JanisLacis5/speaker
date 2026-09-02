#ifndef TASK_HANDLER_H
#define TASK_HANDLER_H

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char* TH_TAG = "TASK_HANDLER";

enum class app_signal : uint8_t {
    BT_APP_SIG_WORK_DISPATCH,
    BT_APP_SIG_BAD,
};

enum {
    BT_APP_EVT_STACK_UP,
};

class task 
{
public:
    explicit task(app_signal signal, uint16_t event)
        : signal{signal}, event_{event}
    {}
    virtual ~task() = default;
    virtual void execute() = 0;

    app_signal signal{app_signal::BT_APP_SIG_BAD};

protected:
    uint16_t event_;
};

template <typename T>
class bt_app_msg : public task
{
public:
    using callback_t = void(*)(uint16_t event, T* parms);

    explicit bt_app_msg(callback_t cb, T* parms, uint16_t event, app_signal signal) 
        : task{signal, event}, callback_{cb}
    {
        callback_parms_ = new T(*parms);
    }

    ~bt_app_msg() {
        if (callback_parms_)
            delete callback_parms_;
    }

    // todo: add copy assignment operator and move constructor/operator
    bt_app_msg(const bt_app_msg& other)
        : callback_{other.callback_}
    {
        callback_parms_ = new T(*other.callback_parms_);
    }

    void execute() override { 
        ESP_LOGI(TH_TAG, "%s, signal: 0x%x, event: 0x%x", __func__, signal, event_);

        if (callback_) {
            callback_(event_, callback_parms_); 
        } else {
            ESP_LOGE(TH_TAG, "callback not provided to bt_app_msg");
        }
    }

private:
    callback_t callback_;
    T* callback_parms_;
};

template <>
class bt_app_msg<void> : public task
{
public:
    using callback_t = void(*)(uint16_t event);

    explicit bt_app_msg(callback_t cb, uint16_t event, app_signal signal) 
        : task{signal, event}, callback_{cb}
    {
        ESP_LOGI(TH_TAG, "templateless bt_app_msg, signal: %d, event: %d", signal, event);
    }

    void execute() override { 
        ESP_LOGI(TH_TAG, "%s, signal: 0x%x, event: 0x%x", __func__, signal, event_);

        if (callback_) {
            callback_(event_); 
        } else {
            ESP_LOGE(TH_TAG, "callback not provided to bt_app_msg");
        }
    }

private:
    callback_t callback_;
};

// TODO: freeRTOS is executing tasks concurrently, on add/remove task syncronization is needed
class task_handler 
{
public:
    ~task_handler();

    template<typename T, typename ... Args>
    task* add_task(Args... args) {
        auto free_slot_iter = std::find_if(task_slots.begin(), task_slots.end(), [](auto& slot) { return !slot.occupied; });
        if (free_slot_iter == task_slots.end())
            return nullptr;

        auto* ptr = new (free_slot_iter->storage) T(args...);
        free_slot_iter->ptr = ptr;
        free_slot_iter->occupied = true;

        if (xQueueSend(task_queue_, &ptr, 10 / portTICK_PERIOD_MS) != pdTRUE) {
            ESP_LOGE(TH_TAG, "%s xQueue send failed", __func__);
            remove_task(ptr);
            return nullptr;
        }
        return ptr;
    }

    void remove_task(const task* task);

    void init();

private:
    static void main_task(void* arg);

    QueueHandle_t task_queue_;
    TaskHandle_t task_handle_;

    struct task_slot {
        static constexpr size_t size = 128;

        alignas(std::max_align_t) std::byte storage[size];

        task* ptr = nullptr;
        bool occupied = false;
    };

    std::array<task_slot, 16> task_slots;
};

#endif

