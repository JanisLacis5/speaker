#include "esp_err.h"
#include "driver/i2s_std.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "task_handler.hpp"
#include "bt_utils.hpp"

namespace {

// static void i2s_mic_init(void)
// {
//     i2s_chan_config_t chan_cfg =
//         I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
//
//     ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));
//
//     i2s_std_config_t std_cfg = {
//         .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
//
//         .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
//             I2S_DATA_BIT_WIDTH_32BIT,
//             I2S_SLOT_MODE_STEREO
//         ),
//
//         .gpio_cfg = {
//             .mclk = I2S_GPIO_UNUSED,
//             .bclk = I2S_BCLK_IO,
//             .ws   = I2S_WS_IO,
//             .dout = I2S_GPIO_UNUSED,
//             .din  = I2S_DIN_IO,
//             .invert_flags = {
//                 .mclk_inv = false,
//                 .bclk_inv = false,
//                 .ws_inv   = false,
//             },
//         },
//     };
//
//     ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
// }

}

extern "C" void app_main()
{
    static task_handler th{};
    static bt_utils bt{th};

    ESP_ERROR_CHECK(bt.bredr_app_common_init());

    th.init();
    th.add_task<bt_app_msg<void>>(bt.bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, app_signal::BT_APP_SIG_WORK_DISPATCH);
}
