/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
// For ADC input:
#include "hardware/adc.h"
#include "hardware/dma.h"
// For resistor DAC output:
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "pico/binary_info.h"
#include "Adafruit_ZeroFFT.h"
// #include "resistor_dac.pio.h"

// This example uses the DMA to capture many samples from the ADC.
//
// - We are putting the ADC in free-running capture mode at 0.5 Msps
//
// - A DMA channel will be attached to the ADC sample FIFO
//
// - Configure the ADC to right-shift samples to 8 bits of significance, so we
//   can DMA into a byte buffer
//
// This could be extended to use the ADC's round robin feature to sample two
// channels concurrently at 0.25 Msps each.
//
// It would be nice to have some analog samples to measure! This example also
// drives waves out through a 5-bit resistor DAC, as found on the reference
// VGA board. If you have that board, you can take an M-F jumper wire from
// GPIO 26 to the Green pin on the VGA connector (top row, next-but-rightmost
// hole). Or you can ignore that part of the code and connect your own signal
// to the ADC input.

// Channel 0 is GPIO26
#define CAPTURE_CHANNEL 0
#define CAPTURE_DEPTH 1000

uint16_t capture_buf[CAPTURE_DEPTH];
const uint LED_PIN = 25;

void core1_main();

int main() {
    bi_decl(bi_program_description("This is a test binary."));
    bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

    stdio_init_all();

    // Send core 1 off to start driving the "DAC" whilst we configure the ADC.
    // multicore_launch_core1(core1_main);

    // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
    adc_gpio_init(26 + CAPTURE_CHANNEL);

    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        false     // Shift each sample to 8 bits when pushing to FIFO
    );

    // Divisor of 0 -> full speed. Free-running capture with the divider is
    // equivalent to pressing the ADC_CS_START_ONCE button once per `div + 1`
    // cycles (div not necessarily an integer). Each conversion takes 96
    // cycles, so in general you want a divider of 0 (hold down the button
    // continuously) or > 95 (take samples less frequently than 96 cycle
    // intervals). This is all timed by the 48 MHz ADC clock.
    adc_set_clkdiv(0);

    sleep_ms(5000);
    printf("Arming DMA\n");
    sleep_ms(1000);
    // Set up the DMA to start transferring data as soon as it appears in FIFO
    uint dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(dma_chan, &cfg,
        capture_buf,    // dst
        &adc_hw->fifo,  // src
        CAPTURE_DEPTH,  // transfer count
        true            // start immediately
    );

    printf("Starting capture\n");
    adc_run(true);

    // Once DMA finishes, stop any new conversions from starting, and clean up
    // the FIFO in case the ADC was still mid-conversion.
    dma_channel_wait_for_finish_blocking(dma_chan);
    printf("Capture finished\n");
    adc_run(false);
    adc_fifo_drain();

    // Print samples to stdout so you can display them in pyplot, excel, matlab
    for (int i = 0; i < CAPTURE_DEPTH; ++i) {
        const float conversion_factor = 3.3f / (1 << 12);
        printf("Raw value: 0x%03x, voltage: %f V\n", capture_buf[i], capture_buf[i] * conversion_factor);
        //printf("0x%03x, ", capture_buf[i]);
        if (i % 10 == 9)
            printf("\n");
    }

    ZeroFFT((int16_t*)capture_buf, CAPTURE_DEPTH);
}

// ----------------------------------------------------------------------------
// Code for driving the "DAC" output for us to measure

// Core 1 is just going to sit and drive samples out continously. PIO provides
// consistent sample frequency.
/*
#define OUTPUT_FREQ_KHZ 5
#define SAMPLE_WIDTH 5
// This is the green channel on the VGA board
#define DAC_PIN_BASE 6

void core1_main() {
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio0, true);
    uint offset = pio_add_program(pio0, &resistor_dac_5bit_program);
    resistor_dac_5bit_program_init(pio0, sm, offset,
        OUTPUT_FREQ_KHZ * 1000 * 2 * (1 << SAMPLE_WIDTH), DAC_PIN_BASE);
    while (true) {
        // Triangle wave
        for (int i = 0; i < (1 << SAMPLE_WIDTH); ++i)
            pio_sm_put_blocking(pio, sm, i);
        for (int i = 0; i < (1 << SAMPLE_WIDTH); ++i)
            pio_sm_put_blocking(pio, sm, (1 << SAMPLE_WIDTH) - 1 - i);
    }
}
*/

