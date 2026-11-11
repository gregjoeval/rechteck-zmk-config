zephyr_library()
zephyr_library_sources_ifdef(CONFIG_BLE_PROFILE_LED src/ble_profile_led.c)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
