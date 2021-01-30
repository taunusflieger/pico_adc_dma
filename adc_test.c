#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "pico/binary_info.h"

// Channel 0 is GPIO26
#define CAPTURE_CHANNEL 0

#define CAPTURE_DEPTH 20

uint8_t sample_buf[CAPTURE_DEPTH];
uint dma_chan;

// 12-bit conversion, assume max value == ADC_VREF == 3.3 V
const float conversion_factor = 3.3f / (1 << 12);
const uint LED_PIN = 25;
bool data_ready = false;

void __not_in_flash_func(adc_capture)(uint16_t *buf, size_t count) {
    adc_fifo_setup(true, false, 0, false, false);
    adc_run(true);
    for (int i = 0; i < count; i = i + 1)
        buf[i] = adc_fifo_get_blocking();
    adc_run(false);
    adc_fifo_drain();
}




int main()
{
    bi_decl(bi_program_description("This is a test binary."));
    // bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    

    /////////////////
    // ADC configuration
    /////////////////
    
    // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
    adc_gpio_init(26 + CAPTURE_CHANNEL);
    
    adc_init();


    // Select ADC input 0 (GPIO26)
    adc_select_input(CAPTURE_CHANNEL);

    // Enable FIFO setup for DMA
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        CAPTURE_DEPTH/2,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        true     // Shift each sample to 8 bits when pushing to FIFO
    );

    // Divisor of 0 -> full speed. Free-running capture with the divider is
    // equivalent to pressing the ADC_CS_START_ONCE button once per `div + 1`
    // cycles (div not necessarily an integer). Each conversion takes 96
    // cycles, so in general you want a divider of 0 (hold down the button
    // continuously) or > 95 (take samples less frequently than 96 cycle
    // intervals). This is all timed by the 48 MHz ADC clock.
    adc_set_clkdiv(0);

    sleep_ms(10000);
    printf("Arming DMA\n");
    sleep_ms(1000);

 
    /////////////////
    // DMA configuration
    /////////////////

    // Set up the DMA to start transferring data as soon as it appears in FIFO
    dma_chan = dma_claim_unused_channel(true);
    
    // 8 bit transfers. Both read and write address increment after each
    // transfer (each pointing to a location in src or dst respectively).
    // No DREQ is selected, so the DMA transfers as fast as it can.
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
   
    
    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(
        dma_chan,             // Channel to be configured
        &cfg,                 // The configuration we just created
        sample_buf,           // The initial write address
        &adc_hw->fifo,        // ADC's FIFO register as source
        CAPTURE_DEPTH,        // Number of transfers
        true                 // start immediately.
    );


    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    //dma_channel_set_irq0_enabled(dma_chan, true);
    
    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    //irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    //irq_set_enabled(DMA_IRQ_0, true);
    //dma_channel_start(dma_chan);

    // Once DMA finishes, stop any new conversions from starting, and clean up
    // the FIFO in case the ADC was still mid-conversion.
    //dma_channel_wait_for_finish_blocking(dma_chan);
    //printf("Capture finished\n");
    //adc_run(false);
    //adc_fifo_drain();
    
    printf("Starting capture\n");
    adc_run(true);

    // Once DMA finishes, stop any new conversions from starting, and clean up
    // the FIFO in case the ADC was still mid-conversion.
    //while (true) {
        dma_channel_wait_for_finish_blocking(dma_chan);
        printf("Capture finished\n");
        //adc_run(false);
        //adc_fifo_drain();
            
        for (int i = 0; i < CAPTURE_DEPTH; i++)
            printf("Raw value: 0x%03x, voltage: %f V\n", sample_buf[i], sample_buf[i] * conversion_factor);
   // }
    return 0;
}
