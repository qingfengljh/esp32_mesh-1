deps_config := \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/app_trace/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/aws_iot/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/bt/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/driver/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/third_party/esp-aliyun/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/esp32/Kconfig \
	/Users/duanshuai/esp/esp32/components/esp32-camera/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/esp_adc_cal/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/esp_event/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/esp_http_client/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/esp_http_server/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/ethernet/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/fatfs/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/freemodbus/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/freertos/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/heap/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/libsodium/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/log/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/lwip/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/maliyun_linkkit/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/mbedtls/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/mcommon/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/mconfig/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/mdebug/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/mdns/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/mespnow/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/third_party/miniz/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/mqtt/Kconfig \
	/Users/duanshuai/esp/esp32/components/mupgrade/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/mwifi/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/nvs_flash/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/openssl/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/pthread/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/spi_flash/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/spiffs/Kconfig \
	/Users/duanshuai/esp/esp-mdf/components/third_party/esp-aliyun/components/ssl/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/tcpip_adapter/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/vfs/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/wear_levelling/Kconfig \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/bootloader/Kconfig.projbuild \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/esptool_py/Kconfig.projbuild \
	/Users/duanshuai/esp/esp32/main/Kconfig.projbuild \
	/Users/duanshuai/esp/esp-mdf/esp-idf/components/partition_table/Kconfig.projbuild \
	/Users/duanshuai/esp/esp-mdf/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
