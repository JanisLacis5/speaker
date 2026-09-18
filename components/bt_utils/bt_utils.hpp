#ifndef BT_UTILS_H
#define BT_UTILS_H

#include "esp_err.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"

#include "task_handler.hpp"
#include "audio_sink.hpp"

class bt_utils 
{
public: 
    bt_utils(task_handler& th) 
        : th_{th}
    {
        instance_ = this;
    }

    ~bt_utils() {
        instance_ = nullptr;
    }

    esp_err_t bredr_app_common_init();
    static void bt_av_hdl_stack_evt(std::uint16_t event);
private:
    audio_sink audio{};
    task_handler& th_;
    std::uint32_t pkt_cnt_{0}; 

    void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t* param);
    void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param);
    void bt_app_a2d_data_cb(const std::uint8_t* data, std::uint32_t len);

    void bt_a2d_evt_def_hdl(std::uint16_t event, esp_a2d_cb_param_t* param);
    void bt_a2d_evt_int_codec_hdl(std::uint16_t event, esp_a2d_cb_param_t* param);

    // helpers to interact with esp C api
    static bt_utils *instance_;
    static void static_bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t* param) {
        if (instance_)
            instance_->bt_app_dev_cb(event, param);
    }
    static void static_bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
        if (instance_)
            instance_->bt_app_gap_cb(event, param);
    }
    static void static_bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
        if (instance_)
            instance_->bt_app_a2d_cb(event, param);
    }
    static void static_bt_app_a2d_data_cb(const std::uint8_t* data, std::uint32_t len) {
        if (instance_)
            instance_->bt_app_a2d_data_cb(data, len);
    }
    static void static_bt_a2d_evt_def_hdl(std::uint16_t event, esp_a2d_cb_param_t* param) {
        if (instance_)
            instance_->bt_a2d_evt_def_hdl(event, param);
    }
    static void static_bt_a2d_evt_int_codec_hdl(std::uint16_t event, esp_a2d_cb_param_t* param) {
        if (instance_)
            instance_->bt_a2d_evt_int_codec_hdl(event, param);
    }
};

#endif
