/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <nrf9160.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <string.h>
#include <stdlib.h>
#include <nrfx_pdm.h>
//#include <nrfx_timer.h>
//#include <nrfx_dppi.h>
#include <nrfx_gpiote.h>
#include <hal/nrf_gpiote.h>
//#include <hal/nrf_timer.h>

#include <drivers/gpio.h>

#define PDM_BUFFER_NUMBER   2
#define PDM_BUFFER_SIZE_SAMPLES   NRFX_PDM_MAX_BUFFER_SIZE/4
static int16_t pdm_buffer[PDM_BUFFER_SIZE_SAMPLES * PDM_BUFFER_NUMBER];  // 8KB x2 buffer

ISR_DIRECT_DECLARE(pdm_isr_handler)
{
	nrfx_pdm_irq_handler();
	ISR_DIRECT_PM(); /* PM done after servicing interrupt for best latency */
	return 1; /* We should check if scheduling decision should be made */
}

void pdm_enable(void)
{
    nrfx_err_t status;

    // Turn on microphone power.
#if CONFIG_PDM_IO_MIC_ACTIVE_LOW
    nrf_gpio_pin_clear(CONFIG_PDM_IO_MIC);
#else
    nrf_gpio_pin_set(CONFIG_PDM_IO_MIC);
#endif

    // Start audio capture.
    status = nrfx_pdm_start();
    if (status != NRFX_SUCCESS) {
        printk("PDM start failed (%d)\n", status);
    }

    printk("PDM started\n");
}

void pdm_disable(void)
{
    nrfx_err_t status;

    // Stop audio capture.
    status = nrfx_pdm_stop();
    if (status != NRFX_SUCCESS) {
        printk("PDM stop failed (%d)\r\n", status);
        return;
    }

    // Turn off microphone power.
#if CONFIG_PDM_IO_MIC_ACTIVE_LOW
    nrf_gpio_pin_set(CONFIG_PDM_IO_MIC);
#else
    nrf_gpio_pin_clear(CONFIG_PDM_IO_MIC);
#endif

    printk("PDM stopped\n");
}

static void pdm_event_handler(nrfx_pdm_evt_t const * const p_evt)
{
    nrfx_err_t status;
    static int buf_index;

    printk("PDM error flag %d\n", p_evt->error);

    if (p_evt->buffer_requested) {
	int16_t offset = PDM_BUFFER_SIZE_SAMPLES * (buf_index % PDM_BUFFER_NUMBER);
        status = nrfx_pdm_buffer_set(pdm_buffer + offset, PDM_BUFFER_SIZE_SAMPLES);
        if (status != NRFX_SUCCESS) {
            printk("PDM set set buffer failed (%d)\n", status);
        }
    }

    if (p_evt->buffer_released) {
        /* get input data here*/
        for(int i=0; i < 16; i++)
        {
          printk("0x%04X ", p_evt->buffer_released[i]);
        }
        printk("\n");
        /* prepare for next receiving buffer */
        buf_index++;
    }
}

int pdm_config(void)
{
    nrfx_err_t status;
    nrfx_pdm_config_t pdm_cfg = NRFX_PDM_DEFAULT_CONFIG(CONFIG_PDM_IO_CLK, CONFIG_PDM_IO_DATA);

    // Turn off microphone power.
#if CONFIG_PDM_IO_MIC_ACTIVE_LOW
    nrf_gpio_pin_set(CONFIG_PDM_IO_MIC);
#else
    nrf_gpio_pin_clear(CONFIG_PDM_IO_MIC);
#endif
    nrf_gpio_cfg_output(CONFIG_PDM_IO_MIC);

    // Initialize PDM driver.
    status = nrfx_pdm_init(&pdm_cfg, pdm_event_handler);
    if (status != NRFX_SUCCESS) {
        printk("PDM init failed (%d)\n", status);
    }

    return status;
}

void main(void)
{
    printk("Starting nrfx pdm sample!\n");
    IRQ_DIRECT_CONNECT(PDM_IRQn, 0, pdm_isr_handler, 0);
    if (pdm_config() == NRFX_SUCCESS) {
        pdm_enable();
    }
}