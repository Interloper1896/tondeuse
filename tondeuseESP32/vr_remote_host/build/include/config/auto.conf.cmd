deps_config := \
	/home/louis/esp/esp-idf/components/app_trace/Kconfig \
	/home/louis/esp/esp-idf/components/aws_iot/Kconfig \
	/home/louis/esp/esp-idf/components/bt/Kconfig \
	/home/louis/esp/esp-idf/components/driver/Kconfig \
	/home/louis/esp/esp-idf/components/esp32/Kconfig \
	/home/louis/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/louis/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/louis/esp/esp-idf/components/ethernet/Kconfig \
	/home/louis/esp/esp-idf/components/fatfs/Kconfig \
	/home/louis/esp/esp-idf/components/freertos/Kconfig \
	/home/louis/esp/esp-idf/components/heap/Kconfig \
	/home/louis/esp/esp-idf/components/http_server/Kconfig \
	/home/louis/esp/esp-idf/components/libsodium/Kconfig \
	/home/louis/esp/esp-idf/components/log/Kconfig \
	/home/louis/esp/esp-idf/components/lwip/Kconfig \
	/home/louis/esp/esp-idf/components/mbedtls/Kconfig \
	/home/louis/esp/esp-idf/components/mdns/Kconfig \
	/home/louis/esp/esp-idf/components/mqtt/Kconfig \
	/home/louis/esp/esp-idf/components/openssl/Kconfig \
	/home/louis/esp/esp-idf/components/pthread/Kconfig \
	/home/louis/esp/esp-idf/components/spi_flash/Kconfig \
	/home/louis/esp/esp-idf/components/spiffs/Kconfig \
	/home/louis/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/louis/esp/esp-idf/components/vfs/Kconfig \
	/home/louis/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/louis/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/louis/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/louis/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/louis/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
