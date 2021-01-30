#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/binary_info.h"

int main()
{
    bi_decl(bi_program_description("This is a test binary."));
    // bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

    // Data will be copied from src to dst
    const char src[] = "Hello, world! (from DMA) %d\n";
    char dst[count_of(src)];
    uint32_t count = 0;

    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    stdio_init_all();

    // Get a free channel, panic() if there are none
    int chan = dma_claim_unused_channel(true);


    while (true) {
      
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
   
   
       
        // 8 bit transfers. Both read and write address increment after each
        // transfer (each pointing to a location in src or dst respectively).
        // No DREQ is selected, so the DMA transfers as fast as it can.

        dma_channel_config c = dma_channel_get_default_config(chan);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, true);

        dma_channel_configure(
            chan,          // Channel to be configured
            &c,            // The configuration we just created
            dst,           // The initial write address
            src,           // The initial read address
            count_of(src), // Number of transfers; in this case each is 1 byte.
            true           // Start immediately.
        );

        // We could choose to go and do something else whilst the DMA is doing its
        // thing. In this case the processor has nothing else to do, so we just
        // wait for the DMA to finish.
        dma_channel_wait_for_finish_blocking(chan);
    
        printf(dst, count++);
    }
    return 0;
}
