idf_component_register(
    SRCS "audio_sink.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2s bt esp_ringbuf
)
