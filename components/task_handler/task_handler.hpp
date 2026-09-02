#ifndef TASK_HANDLER_H
#define TASK_HANDLER_H

#include <cstdint>

#include "esp_log.h"

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
        ESP_LOGI("TASK_HANDLER", "%s, signal: 0x%x, event: 0x%x", __func__, signal, event_);

        if (callback_) {
            callback_(event_, callback_parms_); 
        } else {
            ESP_LOGE("TASK_HANDLER", "callback not provided to bt_app_msg");
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
        ESP_LOGI("TASK_HANDLER", "templateless bt_app_msg, signal: %d, event: %d", signal, event);
    }

    void execute() override { 
        ESP_LOGI("TASK_HANDLER", "%s, signal: 0x%x, event: 0x%x", __func__, signal, event_);

        if (callback_) {
            callback_(event_); 
        } else {
            ESP_LOGE("TASK_HANDLER", "callback not provided to bt_app_msg");
        }
    }

private:
    callback_t callback_;
};

#endif
