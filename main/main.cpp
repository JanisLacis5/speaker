#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "task_handler.hpp"
#include "bt_utils.hpp"

extern "C" void app_main()
{
    static task_handler th{};
    static bt_utils bt{th};

    ESP_ERROR_CHECK(bt.bredr_app_common_init());

    th.init();
    th.add_task<bt_app_msg<void>>(bt.bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, app_signal::BT_APP_SIG_WORK_DISPATCH);
}
