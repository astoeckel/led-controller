/*
 *  LED Controller
 *  Copyright (C) 2016 Andreas Stöckel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file uart.hpp
 *
 * Contains the UART driver used on the AVR microcontroller. Currently only
 * tested on an atmega168A.
 *
 * @author Andreas Stöckel
 */

#pragma once

#include <avr/interrupt.h>
#include <avr/io.h>

ISR(USART_RX_vect);
ISR(USART_UDRE_vect);

/**
 * UART driver for the AVR. Implements interrupt based, asynchronous receive and
 * transmit.
 */
class Uart {
private:
	friend void USART_RX_vect();
	friend void USART_UDRE_vect();

	struct Impl {
		static constexpr uint32_t baud_rate = 9600;

		static constexpr uint16_t ubrr =
		    (F_CPU + baud_rate * 8) / (baud_rate * 16) - 1;

		static constexpr uint32_t real_baud_rate = F_CPU / (16 * (ubrr + 1));

		static constexpr uint32_t baud_error =
		    (real_baud_rate * 1000) / baud_rate;

		static_assert((baud_error > 990) && (baud_error < 1010),
		              "Systematic baud rate error larger than 1%!");

		struct Buffer {
			volatile uint8_t buf[16];
			volatile uint8_t rd : 4;
			volatile uint8_t wr : 4;

			Buffer() : rd(0), wr(0) {}

			uint8_t put(const char *b, uint8_t size)
			{
				uint8_t res = size;
				while ((wr + 1) != rd && res) {
					buf[wr] = *b;
					++b;
					++wr;
					--res;
				}
				return size - res;
			}

			uint8_t get(char *b, uint8_t size)
			{
				uint8_t res = size;
				while (rd != wr && res) {
					*b = buf[rd];
					++b;
					++rd;
					--res;
				}
				return size - res;
			}

			uint8_t wr_avail() const { return (wr - rd + 15) & 0x0F; }

			uint8_t rd_avail() const { return (rd - wr) & 0x0F; }
		};

		Buffer rx_buf;
		Buffer tx_buf;

		Impl()
		{
			UBRR0 = ubrr;
			UCSR0B |= (1 << RXCIE0) | (1 << TXEN0) | (1 << RXEN0);
			UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Async, 8N1
		}

		void tx()
		{
			if (UCSR0A & (1 << UDRE0)) {
				char c;
				if (tx_buf.get(&c, 1)) {
					UDR0 = c;
					UCSR0B |= (1 << UDRIE0); // Enable interrupt
				} else {
					UCSR0B &= ~(1 << UDRIE0); // Disable interrupt
				}
			}
		}

		void rx()
		{
			if (rx_buf.wr_avail() > 1) {
				char c = UDR0;
				rx_buf.put(&c, 1);
			}
		}
	};

	static Impl impl;

public:
	static uint8_t put(const char *buf, uint8_t size)
	{
		uint8_t res = impl.tx_buf.put(buf, size);
		impl.tx();
		return res;
	}

	static bool can_put(uint8_t size) { return impl.tx_buf.wr_avail() >= size; }

	static uint8_t get(char *buf, uint8_t size)
	{
		return impl.rx_buf.get(buf, size);
	}

	static bool can_get(uint8_t size) { return impl.rx_buf.rd_avail() >= size; }
};

