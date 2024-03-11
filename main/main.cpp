#include <string>

#include <trielo/trielo.hpp>

#include "sd_card.hpp"

extern "C" void app_main(void) {
    Trielo::trielo<SD_Card::init>(Trielo::OkErrCode(0));
    for(size_t y = 0; y < 1; y++) {
        for(size_t i = 1, stopper = 128 * 1024 * 1024; (2 << i) <= stopper; i++) {
            const size_t size { 2u << i };
            const std::string name { std::string("test").append(std::to_string(i)) };
            if(Trielo::trielo<SD_Card::create_test_file>(Trielo::OkErrCode(0), size, name) != 0) {
                goto failure;
            }
            if(Trielo::trielo<SD_Card::check_test_file>(Trielo::OkErrCode(0), size, name) != 0) {
                goto failure;
            }
        }
    }
failure:
    Trielo::trielo<SD_Card::deinit>(Trielo::OkErrCode(0));
}