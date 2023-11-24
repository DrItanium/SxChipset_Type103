#include <pico/stdlib.h>
#include <hardware/pio.h>
#include "as_detector.pio.h"


int main() {
    PIO pio = pio0;

    uint offset = pio_add_program(pio, &as_detector_program);

    uint sm = pio_claim_unused_sm(pio, true);
    as_detector_program_init(pio, sm, offset, 8, PICO_DEFAULT_LED_PIN);
    while (true) {

    }
    return 0;
}
