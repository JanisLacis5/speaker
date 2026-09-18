idf_component_register(
    SRCS "bt_utils.cpp"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash task_handler 
)
