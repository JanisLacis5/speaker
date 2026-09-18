#ifndef AUDIO_SINK_H
#define AUDIO_SINK_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/i2s_std.h"

class audio_sink
{
public:
    void open();
    void start();
    void stop();
    void close();
    void info_update();
    void data_output();

private:
    static constexpr std::size_t RINGBUF_HIGHEST_WATER_LEVEL = 32 * 1024;

    typedef enum {
        RINGBUFFER_MODE_PROCESSING,    /* ringbuffer is buffering incoming audio data */
        RINGBUFFER_MODE_PREFETCHING,   /* ringbuffer is buffering incoming audio data */
        RINGBUFFER_MODE_DROPPING       /* ringbuffer is not buffering (dropping) incoming audio data */
    } audio_sink_ringbuffer_mode_t;
    typedef enum {
        CHANNEL_STATUS_IDLE,
        CHANNEL_STATUS_OPENED,
        CHANNEL_STATUS_ENABLED
    } audio_sink_chan_st_t;
    typedef struct {
        i2s_chan_handle_t tx_chan;        /* handle of i2s channel */
        audio_sink_chan_st_t chan_st;     /* i2s channel status */
        TaskHandle_t write_task_handle;   /* handle of writing task */
        RingbufHandle_t ringbuf;          /* handle of ringbuffer */
        SemaphoreHandle_t write_semaphore;/* handle of write semaphore */
        uint16_t ringbuffer_mode;         /* ringbuffer mode */
    } audio_sink_srv_i2s_cb_t;

    audio_sink_srv_i2s_cb_t s_i2s_cb;
};

#endif
