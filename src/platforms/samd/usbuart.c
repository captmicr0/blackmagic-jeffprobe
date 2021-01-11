/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2018  Flirc Inc.
 * Written by Jason Kotzin <jasonkotzin@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "general.h"
#include "cdcacm.h"

#include <libopencm3/sam/d/bitfield.h>
#include <libopencm3/sam/d/gclk.h>
#include <libopencm3/sam/d/pm.h>
#include <libopencm3/sam/d/port.h>
#include <libopencm3/sam/d/nvic.h>
#include <libopencm3/sam/d/uart.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/scs.h>
#include <libopencm3/cm3/systick.h>

#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

#include "queue.h"

#define Q_SIZE		1024

/* Active USART number */
static uint8_t USART_NUM = 0;

/* Current Baud Rate setting */
static uint32_t current_baud = 115200;

extern usbd_device * usbdev;

/* input and output ring buffer */
struct {
        char buf[Q_SIZE];
        volatile size_t head, tail;
} rx, tx;

/* non blocking putc function */
static void usart_putc(char c)
{
#ifdef CONSOLE_NO_AUTO_CRLF
	if (c == '\n')
		usart_putc('\r');
#endif

	if (qfull(tx.head, tx.tail, Q_SIZE))
		return;

	cm_disable_interrupts();
	tx.buf[tx.head] = c;
	tx.head = qinc(tx.head, Q_SIZE);
	cm_enable_interrupts();

	/* kick the transmitter to restart interrupts */
	usart_enable_tx_interrupt(USART_NUM);
}

void usbuart_init(void)
{
	/* enable gpios */
	gpio_config_special(PORTA, UART_TX_PIN, UART_PERIPH); /* tx pin */
	gpio_config_special(PORTA, UART_RX_PIN, UART_PERIPH); /* rx pin */

	/* enable clocking to sercom0 */
	set_periph_clk(GCLK0, GCLK_ID_SERCOM0_CORE);
	periph_clk_en(GCLK_ID_SERCOM0_CORE, 1);

	//usart_enable(USART_NUM, current_baud);
	usart_setup(USART_NUM, current_baud);
#ifndef DEBUG_ME
        usart_set_pads(USART_NUM, 3, 0); /* bm-sam uses different pads */
#endif
	usart_enable(USART_NUM, 0); /* baud==0 so setup is skipped */

	usart_enable_rx_interrupt(USART_NUM);
	usart_enable_tx_interrupt(USART_NUM);
}

static uint8_t convert_tdio_enabled;
int usbuart_convert_tdio(uint32_t arg)
{

	(void) arg;

	convert_tdio_enabled = arg;

	if (!convert_tdio_enabled) {
		usart_disable(1);
		USART_NUM = 0;
		usbuart_init();
		return current_baud;
	}

        gpio_config_special(PORTA, TDI_PIN, UART_PERIPH_2); /* TX */
	gpio_config_special(PORTA, TDO_PIN, UART_PERIPH_2); /* RX */

        /* disable USART0 (we will be using USART1 now) */
        usart_disable(0);

	USART_NUM = 1;

        /* Select and Enable system clock */
	set_periph_clk(GCLK0, GCLK_ID_SERCOM1_CORE);
        periph_clk_en(GCLK_ID_SERCOM1_CORE, 1);

	usart_setup(1, current_baud);
        usart_set_pads(1, 3, 0); /* uses different pads than the default */
	usart_enable(1, 0); /* baud==0 so setup is skipped */

        usart_enable_rx_interrupt(1);
        usart_enable_tx_interrupt(1);

	return current_baud;

}

int usbuart_convert_tdio_enabled(void)
{
	return convert_tdio_enabled;
}

void usbuart_set_line_coding(struct usb_cdc_line_coding *coding)
{
	uint8_t sbmode = (coding->bCharFormat == 2) ? 1 : 0;
	uint8_t parity = (coding->bParityType == 1) ? 0 : 1;
	uint8_t form   = (coding->bParityType) ? 1 : 0;
	uint8_t chsize = (form) ? coding->bDataBits + 1 : coding->bDataBits;

	usart_disable(USART_NUM);

	/* set baud rate */
	usart_set_baudrate(USART_NUM, coding->dwDTERate);

	/* set data size, stop mode, and parity */
	usart_set_chsize(USART_NUM, chsize);
	usart_set_sbmode(USART_NUM, sbmode);
	usart_set_parity(USART_NUM, parity, form);

	usart_enable(USART_NUM, 0);

	current_baud = coding->dwDTERate;
}

void usbuart_usb_out_cb(usbd_device *dev, uint8_t ep)
{
	(void)ep;

	char buf[CDCACM_PACKET_SIZE];
	int len = usbd_ep_read_packet(dev, CDCACM_UART_ENDPOINT,
					buf, CDCACM_PACKET_SIZE);

	gpio_set(LED_PORT_UART, LED_UART);
	for(int i = 0; i < len; i++) {
		usart_putc(buf[i]); 
	}
	gpio_clear(LED_PORT_UART, LED_UART);
}

/* run by our systick timer */
void uart_pop(void)
{
	if (cdcacm_get_config() != 1) {
		return;
	}

        if (!qempty(rx.head, rx.tail)) {
		if (usbd_ep_write_packet(usbdev, 0x83, &rx.buf[rx.tail], 1) == 0) {
			return;
		}
		rx.tail = qinc(rx.tail, Q_SIZE);
	}
}

void usbuart_usb_in_cb(usbd_device *dev, uint8_t ep)
{
	(void) dev;
	(void) ep;
}

/************************** UART Interrupt Handlers *************************/
static void uart_rx_irq(void)
{
	char c = UART(USART_NUM)->data;

	/* bug?, need to re-enable rx complete interrupt */
	INSERTBF(UART_INTENSET_RXC, 1, UART(USART_NUM)->intenset);

	if (!qfull(rx.head, rx.tail, Q_SIZE)) {
		rx.buf[rx.head] = c;
		rx.head = qinc(rx.head, Q_SIZE);
	}
}

static void uart_tx_irq(void)
{
	if (!qempty(tx.head, tx.tail)) {
		usart_send(USART_NUM, tx.buf[tx.tail]);
		tx.tail = qinc(tx.tail, Q_SIZE);
	} else {
		usart_disable_tx_interrupt(USART_NUM);
	}
}

void sercom0_isr(void)
{
	/* Turn on LED */
	gpio_set(LED_PORT_UART, LED_UART);

	if (GETBF(UART_INTFLAG_RXC, UART(USART_NUM)->intflag))
		uart_rx_irq();

	if (GETBF(UART_INTFLAG_DRE, UART(USART_NUM)->intflag))
		uart_tx_irq();
}

void sercom1_isr(void)
{

	/* Turn on LED */
	gpio_set(LED_PORT_UART, LED_UART);

	if (GETBF(UART_INTFLAG_RXC, UART(USART_NUM)->intflag))
		uart_rx_irq();

	if (GETBF(UART_INTFLAG_DRE, UART(USART_NUM)->intflag))
		uart_tx_irq();
}
