#include <inttypes.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

constexpr const char BT_APP_CORE_TAG[] = "BT_APP_CORE";
constexpr const char BT_AV_TAG[] = "BT_AV";
constexpr const char local_device_name[] = "JANIS_SPEAKER";
constexpr const char *s_a2d_conn_state_str[] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
constexpr const char *s_a2d_audio_state_str[] = {"Suspended", "Started"};

// TODO
// enum class app_signal : uint8_t {
//     BT_APP_SIG_WORK_DISPATCH,
// };
// enum class app_event: uint8_t {
//     BT_APP_EVT_STACK_UP,
// };

enum {
    BT_APP_SIG_WORK_DISPATCH,
};
enum {
    BT_APP_EVT_STACK_UP,
};

uint32_t s_pkt_cnt = 0; 
QueueHandle_t task_queue = NULL;

// TODO
// typedef void (* bt_app_cb_t)(app_event event, void *param);
typedef void (* bt_app_cb_t)(uint16_t event, void *param);
typedef void (* bt_app_copy_cb_t)(void *p_dest, void *p_src, int len);
typedef struct {
    // TODO
    // app_signal sig;      /*!< signal to bt_app_task */
    // app_event event;    /*!< message event id */
    uint16_t sig;
    uint16_t event;
    bt_app_cb_t    cb;       /*!< context switch callback */
    void           *param;   /*!< parameter area needs to be last */
} bt_app_msg_t;

static void bt_app_work_dispatched(bt_app_msg_t *msg)
{
    if (msg->cb) {
        msg->cb(msg->event, msg->param);
    }
}

static void main_task(void* arg)
{
    // QueueHandle_t bt_app_task_queue = static_cast<QueueHandle_t>(arg);
    bt_app_msg_t msg;

    for (;;) {
        /* receive message from work queue and handle it */
        if (pdTRUE == xQueueReceive(task_queue, &msg, (TickType_t)portMAX_DELAY)) {
            ESP_LOGD(BT_APP_CORE_TAG, "%s, signal: 0x%x, event: 0x%x", __func__, msg.sig, msg.event);

            switch (msg.sig) {
            case BT_APP_SIG_WORK_DISPATCH:
                bt_app_work_dispatched(&msg);
                break;
            default:
                ESP_LOGW(BT_APP_CORE_TAG, "%s, unhandled signal: %d", __func__, msg.sig);
                break;
            } /* switch (msg.sig) */

            if (msg.param) {
                free(msg.param);
            }
        }
    }

}

static void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_DEV_NAME_RES_EVT: {
        if (param->name_res.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_APP_CORE_TAG, "Get local device name success: %s", param->name_res.name);
        } else {
            ESP_LOGE(BT_APP_CORE_TAG, "Get local device name failed, status: %d", param->name_res.status);
        }
        break;
    }
    default: {
        ESP_LOGI(BT_APP_CORE_TAG, "event: %d", event);
        break;
    }
    }
}

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    uint8_t *bda = NULL;

    switch (event) {
    /* when authentication completed, this event comes */
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_APP_CORE_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            ESP_LOG_BUFFER_HEX(BT_APP_CORE_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        } else {
            ESP_LOGE(BT_APP_CORE_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
        }
        ESP_LOGI(BT_APP_CORE_TAG, "link key type of current link is: %d", param->auth_cmpl.lk_type);
        break;
    }
    case ESP_BT_GAP_ENC_CHG_EVT: {
        const char *str_enc[3] = {"OFF", "E0", "AES"};
        bda = (uint8_t *)param->enc_chg.bda;
        ESP_LOGI(BT_APP_CORE_TAG, "Encryption mode to [%02x:%02x:%02x:%02x:%02x:%02x] changed to %s",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], str_enc[param->enc_chg.enc_mode]);
        break;
    }

    /* when Security Simple Pairing user confirmation requested, this event comes */
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_APP_CORE_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    /* when Security Simple Pairing passkey notified, this event comes */
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_APP_CORE_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %06" PRIu32, param->key_notif.passkey);
        break;
    /* when Security Simple Pairing passkey requested, this event comes */
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_APP_CORE_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;

    /* when GAP mode changed, this event comes */
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(BT_APP_CORE_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d, interval: %.2f ms",
                 param->mode_chg.mode, param->mode_chg.interval * 0.625);
        break;
    /* when ACL connection completed, this event comes */
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        bda = (uint8_t *)param->acl_conn_cmpl_stat.bda;
        ESP_LOGI(BT_APP_CORE_TAG, "ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT Connected to [%02x:%02x:%02x:%02x:%02x:%02x], status: 0x%x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_conn_cmpl_stat.stat);
        break;
    /* when ACL disconnection completed, this event comes */
    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
        bda = (uint8_t *)param->acl_disconn_cmpl_stat.bda;
        ESP_LOGI(BT_APP_CORE_TAG, "ESP_BT_GAP_ACL_DISC_CMPL_STAT_EVT Disconnected from [%02x:%02x:%02x:%02x:%02x:%02x], reason: 0x%x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_disconn_cmpl_stat.reason);
        break;
    /* others */
    default: {
        ESP_LOGI(BT_APP_CORE_TAG, "event: %d", event);
        break;
    }
    }
}

void bt_a2d_evt_def_hdl(uint16_t event, void *param)
{
    ESP_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(param);

    switch (event) {
    /* when connection state changed, this event comes */
    case ESP_A2D_CONNECTION_STATE_EVT: {
        uint8_t *bda = a2d->conn_stat.remote_bda;
        ESP_LOGI(BT_AV_TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_a2d_conn_state_str[a2d->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        }
        break;
    }
    /* when audio stream transmission state changed, this event comes */
    case ESP_A2D_AUDIO_STATE_EVT: {
        ESP_LOGI(BT_AV_TAG, "A2DP audio state: %s", s_a2d_audio_state_str[a2d->audio_stat.state]);
        break;
    }
    /* when audio codec is configured, this event comes */
    case ESP_A2D_AUDIO_CFG_EVT: {
        esp_a2d_mcc_t *p_mcc = &a2d->audio_cfg.mcc;
        ESP_LOGI(BT_AV_TAG, "A2DP audio stream configuration, codec type: %d", p_mcc->type);
        break;
    }
    /* when a2dp init or deinit completed, this event comes */
    case ESP_A2D_PROF_STATE_EVT: {
        if (ESP_A2D_INIT_SUCCESS == a2d->a2d_prof_stat.init_state) {
            ESP_LOGI(BT_AV_TAG, "A2DP PROF STATE: Init Complete");
        } else {
            ESP_LOGI(BT_AV_TAG, "A2DP PROF STATE: Deinit Complete");
        }
        break;
    }
    /* when using external codec, after sep registration done, this event comes */
    case ESP_A2D_SEP_REG_STATE_EVT: {
        if (a2d->a2d_sep_reg_stat.reg_state == ESP_A2D_SEP_REG_SUCCESS) {
            ESP_LOGI(BT_AV_TAG, "A2DP register SEP success, seid: %d", a2d->a2d_sep_reg_stat.seid);
        } else {
            ESP_LOGI(BT_AV_TAG, "A2DP register SEP fail, seid: %d, state: %d", a2d->a2d_sep_reg_stat.seid, a2d->a2d_sep_reg_stat.reg_state);
        }
        break;
    }
    /* When protocol service capabilities configured, this event comes */
    case ESP_A2D_SNK_PSC_CFG_EVT: {
        ESP_LOGI(BT_AV_TAG, "protocol service capabilities configured: 0x%x ", a2d->a2d_psc_cfg_stat.psc_mask);
        if (a2d->a2d_psc_cfg_stat.psc_mask & ESP_A2D_PSC_DELAY_RPT) {
            ESP_LOGI(BT_AV_TAG, "Peer device support delay reporting");
        } else {
            ESP_LOGI(BT_AV_TAG, "Peer device unsupported delay reporting");
        }
        break;
    }
    /* when set delay value completed, this event comes */
    case ESP_A2D_SNK_SET_DELAY_VALUE_EVT: {
        if (ESP_A2D_SET_INVALID_PARAMS == a2d->a2d_set_delay_value_stat.set_state) {
            ESP_LOGI(BT_AV_TAG, "Set delay report value: fail");
        } else {
            ESP_LOGI(BT_AV_TAG, "Set delay report value: success, delay_value: %u * 1/10 ms", a2d->a2d_set_delay_value_stat.delay_value);
        }
        break;
    }
    /* when get delay value completed, this event comes */
    case ESP_A2D_SNK_GET_DELAY_VALUE_EVT: {
        ESP_LOGI(BT_AV_TAG, "Get delay report value: delay_value: %u * 1/10 ms", a2d->a2d_get_delay_value_stat.delay_value);
        /* Default delay value plus delay caused by application layer */
        esp_a2d_sink_set_delay_value(a2d->a2d_get_delay_value_stat.delay_value + 50);
        break;
    }
    /* others */
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

void bt_a2d_evt_int_codec_hdl(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(param);

    switch (event) {
    /* when connection state changed, this event comes */
    case ESP_A2D_CONNECTION_STATE_EVT: {
        uint8_t *bda = a2d->conn_stat.remote_bda;
        ESP_LOGI(BT_AV_TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_a2d_conn_state_str[a2d->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            // audio_sink_srv_stop();
            // audio_sink_srv_close();
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            // audio_sink_srv_start();
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
            // audio_sink_srv_open();
        }
        break;
    }
    /* when audio stream transmission state changed, this event comes */
    case ESP_A2D_AUDIO_STATE_EVT: {
        ESP_LOGI(BT_AV_TAG, "A2DP audio state: %s", s_a2d_audio_state_str[a2d->audio_stat.state]);
        if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
            s_pkt_cnt = 0;
        }
        break;
    }
    /* when audio codec is configured, this event comes */
    case ESP_A2D_AUDIO_CFG_EVT: {
        // audio_sink_srv_codec_info_update(&a2d->audio_cfg.mcc);
        break;
    }
    /* others */
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

static bool bt_app_send_msg(bt_app_msg_t *msg)
{
    if (msg == NULL) {
        return false;
    }

    /* send the message to work queue */
    if (xQueueSend(task_queue, msg, 10 / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE(BT_APP_CORE_TAG, "%s xQueue send failed", __func__);
        return false;
    }
    return true;
}

bool bt_app_work_dispatch(bt_app_cb_t p_cback, uint16_t event, void *p_params, int param_len, bt_app_copy_cb_t p_copy_cback)
{
    ESP_LOGD(BT_APP_CORE_TAG, "%s event: 0x%x, param len: %d", __func__, event, param_len);

    bt_app_msg_t msg;
    memset(&msg, 0, sizeof(bt_app_msg_t));

    msg.sig = BT_APP_SIG_WORK_DISPATCH;
    msg.event = event;
    msg.cb = p_cback;

    if (param_len == 0) {
        return bt_app_send_msg(&msg);
    } else if (p_params && param_len > 0) {
        if ((msg.param = malloc(param_len)) != NULL) {
            memcpy(msg.param, p_params, param_len);
            /* check if caller has provided a copy callback to do the deep copy */
            if (p_copy_cback) {
                p_copy_cback(msg.param, p_params, param_len);
            }
            return bt_app_send_msg(&msg);
        }
    }

    return false;
}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_PROF_STATE_EVT:
    case ESP_A2D_SNK_PSC_CFG_EVT:
    case ESP_A2D_SNK_SET_DELAY_VALUE_EVT:
    case ESP_A2D_SNK_GET_DELAY_VALUE_EVT: {
        bt_app_work_dispatch(bt_a2d_evt_def_hdl, event, param, sizeof(esp_a2d_cb_param_t), NULL);
        break;
    }
    case ESP_A2D_CONNECTION_STATE_EVT:
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_SEP_REG_STATE_EVT: {
        bt_app_work_dispatch(bt_a2d_evt_int_codec_hdl, event, param, sizeof(esp_a2d_cb_param_t), NULL);
        break;
    }
    default:
        ESP_LOGE(BT_AV_TAG, "Invalid A2DP event: %d", event);
        break;
    }
}

static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len)
{
    // audio_sink_srv_data_output(data, len);

    /* log the number every 100 packets */
    if (++s_pkt_cnt % 100 == 0) {
        ESP_LOGI(BT_AV_TAG, "Audio packet count: %" PRIu32, s_pkt_cnt);
    }
}

// TODO
// static void bt_av_hdl_stack_evt(app_event event, void *p_param)
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

    // TODO
    // if (event == app_event::BT_APP_EVT_STACK_UP) {
    if (event == BT_APP_EVT_STACK_UP) {
        esp_bt_gap_set_device_name(local_device_name);
        esp_bt_dev_register_callback(bt_app_dev_cb);
        esp_bt_gap_register_callback(bt_app_gap_cb);

        esp_a2d_register_callback(&bt_app_a2d_cb);
        esp_err_t err = esp_a2d_sink_init(); 
        if (err != ESP_OK) {
            ESP_LOGI(BT_AV_TAG, "ERROR: %s", esp_err_to_name(err));
        }
        assert(err == ESP_OK);

        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);

        /* Get the default value of the delay value */
        esp_a2d_sink_get_delay_value();
        /* Get local device name */
        esp_bt_gap_get_device_name();

        /* set discoverable and connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    } else {
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
    }
}

static char *bda2str(uint8_t * bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

esp_err_t bredr_app_common_init(void)
{
    char bda_str[18] = {0};
    /* initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((err = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE("BT_BREDR", "%s initialize controller failed: %s", __func__, esp_err_to_name(err));
        return err;
    }
    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE("BT_BREDR", "%s enable controller failed: %s", __func__, esp_err_to_name(err));
        return err;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    if ((err = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE("BT_BREDR", "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(err));
        return err;
    }

    if ((err = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE("BT_BREDR", "%s enable bluedroid failed: %s", __func__, esp_err_to_name(err));
        return err;
    }

    /* set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    /* set default parameters for Legacy Pairing (use fixed pin code 2468) */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '2';
    pin_code[1] = '4';
    pin_code[2] = '6';
    pin_code[3] = '8';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    ESP_LOGI("BT_BREDR", "Own address:[%s]", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));

    return ESP_OK;
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(bredr_app_common_init());

    task_queue = xQueueCreate(10, sizeof(bt_app_msg_t));
    TaskHandle_t task_handle = NULL;
    xTaskCreate(main_task, "BtAppTask", 3072, NULL, 10, &task_handle);

    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);
}
