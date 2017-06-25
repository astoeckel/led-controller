/*
 *  LED Constant Current Source -- AVR Controller
 *  (c) Andreas Stöckel, 2016-2017
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>

#include "bus.hpp"
#include "uart.hpp"

/**
 * Data used for oversampling from 8-bit PWM to 12-bit values
 */
uint16_t pwm_oversample_data[16] = {
    0b0000'0000'0000'0000, 0b1000'0000'0000'0000, 0b1000'0000'1000'0000,
    0b1000'1000'1000'0000, 0b1000'1000'1000'1000, 0b1001'1000'1000'1000,
    0b1001'1000'1001'1000, 0b1001'1000'1001'1001, 0b1001'1001'1001'1001,
    0b1101'1001'1001'1001, 0b1101'1001'1101'1001, 0b1101'1101'1101'1001,
    0b1101'1101'1101'1101, 0b1111'1101'1101'1101, 0b1111'1101'1111'1101,
    0b1111'1111'1111'1101,
};

/**
 * Structure holding the current state of each LED channel.
 */
struct led_channel {
	volatile uint16_t cur_value;
	uint16_t tar_value;
	uint8_t ramp;

	/**
	 * Returns true if the
	 */
	bool is_on(uint8_t phase = 0) const { return cur_value > 0; }

	/**
	 * Sets the target value to the given value.
	 */
	void set_tar_value(uint16_t v)
	{
		// Clamp the given value to 12 bit
		if (v > 0xFFF) {
			v = 0xFFF;
		}

		// Use the entire 16-bit range
		tar_value = v << 4;
		cur_value = v << 4;
	}

	/**
	 * Sets the ramp to the given value.
	 */
	void set_ramp(uint16_t v) { ramp = v; }

	/**
	 * Moves the current value into the direction of the target value.
	 */
	void step()
	{
		if (cur_value > tar_value) {
			if (cur_value - tar_value < ramp) {
				cur_value = tar_value;
			}
			else {
				cur_value -= ramp;
			}
		}
		else {
			if (tar_value - cur_value < ramp) {
				cur_value = tar_value;
			}
			else {
				cur_value += ramp;
			}
		}
	}

	uint8_t pulse_len_for_phase(uint8_t phase = 0) const
	{
		return (cur_value >> 8) +
		       ((pwm_oversample_data[cur_value >> 4 & 0xF] >> phase) & 1);
	}
};

static led_channel channels[4];

static volatile uint8_t phase = 0;

ISR(TIMER0_OVF_vect)
{
	auto toggle_bit = [](volatile uint8_t &v, uint8_t bit, bool value) {
		if (value) {
			v |= (1 << bit);
		}
		else {
			v &= ~(1 << bit);
		}
	};

	// Update the current phase, update the current led channel brightness
	// all 16 steps
	phase = (phase + 1) & 0x0F;
	if (phase == 0) {
		for (uint8_t i = 0; i < 4; i++) {
			channels[i].step();
		}
	}

	// Dis- or enable hardware PWM depending on the channel state
	toggle_bit(TCCR0A, COM0A1, channels[0].is_on(phase));
	toggle_bit(TCCR0A, COM0B1, channels[1].is_on(phase));
	toggle_bit(TCCR1A, COM1B1, channels[2].is_on(phase));
	toggle_bit(TCCR1A, COM1A1, channels[3].is_on(phase));

	// Update the PWM value
	OCR0A = channels[0].pulse_len_for_phase(phase);
	OCR0B = channels[1].pulse_len_for_phase(phase);
	OCR1B = channels[2].pulse_len_for_phase(phase);
	OCR1A = channels[3].pulse_len_for_phase(phase);
}

int main()
{
	// Setup the output IO ports
	PORTB = 0x00;
	PORTD = 0x00;
	DDRB = (1 << 2) | (1 << 1);
	DDRD = (1 << 6) | (1 << 5);

	// Setup PWM
	OCR0A = 0;
	OCR0B = 0;
	OCR1A = 0;
	OCR1B = 0;
	TCCR0A = (1 << WGM00) | (1 << WGM01);
	TCCR0B = (1 << CS01);
	TCCR1A = (1 << WGM10);
	TCCR1B = (1 << WGM12) | (1 << CS11);
	TIMSK0 = (1 << TOIE0);

	// Instantiate the UART
	Uart uart;
	BusClient bus;
	sei();  // Enable interrupts for the message bus and the PWM timer

	// Process incomming messages
	while (true) {
		if (bus.process(uart)) {
			const Message &msg = bus.msg();
			switch (msg.cmd) {
				case Message::CMD_VAL: {
					const uint8_t channel = msg.payload >> 12;
					if (channel < 4) {
						channels[channel].set_tar_value(msg.payload & 0xFFF);
					}
					break;
				}
				case Message::CMD_RAMP: {
					const uint8_t channel = msg.payload >> 12;
					if (channel < 4) {
						channels[channel].set_ramp(msg.payload & 0xFFF);
					}
					break;
				}
			}
		}
	}
}
