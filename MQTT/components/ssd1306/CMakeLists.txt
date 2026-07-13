set(component_srcs "ssd1306.c" "ssd1306_spi.c")
# Dependencias PUBLICAS: os headers driver/spi_master.h e driver/i2c_master.h
# sao incluidos no header publico ssd1306.h, entao os include dirs precisam
# ser propagados para quem inclui esse header (ex.: o componente main).
set(component_requires driver esp_driver_spi esp_driver_i2c esp_driver_gpio)

# get IDF version for comparison
set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

if(idf_version VERSION_GREATER_EQUAL "5.2")
	if(CONFIG_LEGACY_DRIVER)
		list(APPEND component_srcs "ssd1306_i2c_legacy.c")
	else()
		list(APPEND component_srcs "ssd1306_i2c_new.c")
	endif()
else()
	list(APPEND component_srcs "ssd1306_i2c_legacy.c")
endif()

idf_component_register(SRCS "${component_srcs}" REQUIRES ${component_requires} INCLUDE_DIRS ".")
