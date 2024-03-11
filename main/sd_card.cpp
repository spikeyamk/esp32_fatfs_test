#include <thread>
#include <chrono>
#include <filesystem>

#include <driver/gpio.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>
#include <trielo/trielo.hpp>

#include "sd_card.hpp"

namespace SD_Card {
    namespace Pins {
        static const gpio_num_t miso { GPIO_NUM_15 };
        static const gpio_num_t mosi { GPIO_NUM_23 };
        static const gpio_num_t clk  { GPIO_NUM_22 };
        static const gpio_num_t cs   { GPIO_NUM_21 };
    }

    sdmmc_card_t* card { nullptr };

    const esp_vfs_fat_mount_config_t fatfs_conf {
        .format_if_mount_failed = true,
        .max_files = 3,
        .allocation_unit_size = 0,
        .disk_status_check_enable = false,
    };

    // This is just = SDSPI_HOST_DEFAULT() but max_freq_khz modified
    static const sdmmc_host_t host {
        .flags = SDMMC_HOST_FLAG_SPI | SDMMC_HOST_FLAG_DEINIT_ARG,
        .slot = SDSPI_DEFAULT_HOST,
        .max_freq_khz = 20'000,
        .io_voltage = 3.3f,
        .init = &sdspi_host_init,
        .set_bus_width = NULL,
        .get_bus_width = NULL,
        .set_bus_ddr_mode = NULL,
        .set_card_clk = &sdspi_host_set_card_clk,
        .set_cclk_always_on = NULL,
        .do_transaction = &sdspi_host_do_transaction,
        .deinit_p = &sdspi_host_remove_device,
        .io_int_enable = &sdspi_host_io_int_enable,
        .io_int_wait = &sdspi_host_io_int_wait,
        .command_timeout_ms = 0,
        .get_real_freq = &sdspi_host_get_real_freq,
        .input_delay_phase = SDMMC_DELAY_PHASE_0,
        .set_input_delay = NULL
    };


    static const spi_bus_config_t bus_cfg {
        .mosi_io_num = Pins::mosi,
        .miso_io_num = Pins::miso,
        .sclk_io_num = Pins::clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 4092,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_0,
        .intr_flags = 0,
    };

    // This corresponds to SDSPI_DEVICE_CONFIG_DEFAULT() just .host_id and .gpio_cs is modified
    static const sdspi_device_config_t slot_config {
        .host_id   = spi_host_device_t(host.slot),
        .gpio_cs   = Pins::cs,
        .gpio_cd   = SDSPI_SLOT_NO_CD,
        .gpio_wp   = SDSPI_SLOT_NO_WP,
        .gpio_int  = SDSPI_SLOT_NO_INT,
        .gpio_wp_polarity = SDSPI_IO_ACTIVE_LOW,
    };

    sdspi_dev_handle_t handle;

    int init() {
        if(Trielo::trielo<spi_bus_initialize>(Trielo::OkErrCode(ESP_OK), slot_config.host_id, &bus_cfg, SDSPI_DEFAULT_DMA)) {
            return -1;
        }

        if(Trielo::trielo<sdspi_host_init>(Trielo::OkErrCode(ESP_OK)) != ESP_OK) {
            return -2;
        }

        if(Trielo::trielo<esp_vfs_fat_sdspi_mount>(Trielo::OkErrCode(ESP_OK), mount_point.data(), &host, &slot_config, &fatfs_conf, &card) != ESP_OK) {
            return -3;
        }

        Trielo::trielo<sdmmc_card_print_info>(stdout, card);

        return 0;
    }

    esp_err_t deinit() {
        esp_err_t ret { Trielo::trielo<esp_vfs_fat_sdcard_unmount>(Trielo::OkErrCode(ESP_OK), mount_point.data(), card) };
        if(ret != ESP_OK) {
            return ret;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return Trielo::trielo<spi_bus_free>(Trielo::OkErrCode(ESP_OK), slot_config.host_id);
    }

    esp_err_t format() {
        return Trielo::trielo<esp_vfs_fat_sdcard_format>(Trielo::OkErrCode(ESP_OK), mount_point.data(), card);
    }

    int create_test_file(const size_t size_bytes, const std::string_view& name) {
        const std::filesystem::path test_megabyte { std::filesystem::path(mount_point).append(name) };

        FILE* file { std::fopen(test_megabyte.c_str(), "w") };
        if(file == nullptr) {
            return -1;
        }

        char c { 'a' };
        for(
            size_t i = 1;
            i <= size_bytes;
            [&i, &c]() {
                if(c == 'z') {
                    c = 'a';
                } else {
                    c++;
                }

                i++;
            }()
        ) {
            if((i % 1024) == 0) {
                std::fclose(file);
                file = std::fopen(test_megabyte.c_str(), "a");
                if(file == nullptr) {
                    return -2 * static_cast<int>(i);
                }
            }
            if(std::fputc(c, file) != c) {
                return static_cast<int>(i);
            }
        }
        std::fclose(file);
        return 0;
    }

    int check_test_file(const size_t size_bytes, const std::string_view& name) {
        const std::filesystem::path test_megabyte { std::filesystem::path(mount_point).append(name) };

        FILE* file { std::fopen(test_megabyte.c_str(), "r") };
        if(file == nullptr) {
            return -1;
        }

        size_t i = 1;
        for(
            char c { 'a' };
            i <= size_bytes;
            [&]() {
                if(c == 'z') {
                    c = 'a';
                } else {
                    c++;
                }
                i++;
            }()
        ) {
            if(std::fgetc(file) != c) {
                std::fclose(file);
                return static_cast<int>(i + 1);
            }
        }

        if(std::fclose(file) != 0) {
            return -2;
        }

        return 0;
    }
}