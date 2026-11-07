idf_component_register(SRCS "main.c" REQUIRES esp_driver_i2c)

if(DEFINED BMX_I2C_TARGET_INTERNAL_PULLUP)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BMX_I2C_TARGET_INTERNAL_PULLUP=${BMX_I2C_TARGET_INTERNAL_PULLUP})
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        BMX_I2C_TARGET_INTERNAL_PULLUP=1)
endif()
