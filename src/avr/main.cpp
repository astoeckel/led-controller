#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "uart.hpp"

/**
 * Implementation of the communication bus.
 */
class Bus {
private:
	static constexpr uint16_t SYNC_WORD = 0xAFFE;
	static constexpr uint8_t STATE_SYNC = 0;
	static constexpr uint8_t STATE_TAR = 1;
	static constexpr uint8_t STATE_CMD = 2;
	static constexpr uint8_t STATE_PAYLOAD_H = 3;
	static constexpr uint8_t STATE_PAYLOAD_L = 4;
	static constexpr uint8_t STATE_DONE = 5;

	struct Message {
		uint8_t tar;
		uint8_t cmd;
		uint16_t payload;
	};

	uint8_t m_state;
	uint16_t m_sync;
	Message m_msg;

public:
	bool push_byte(uint8_t b)
	{
		switch (m_state) {
		case STATE_SYNC:
			m_sync = (m_sync << 8) | b;
			if (m_sync == SYNC_WORD) {
				m_state++;
			}
			break;
		case STATE_TAR:
			m_msg.tar = b;
			m_state++;
			break;
		case STATE_CMD:
			m_msg.cmd = b;
			m_state++;
			break;
		case STATE_PAYLOAD_H:
		case STATE_PAYLOAD_L:
			m_msg.payload = (m_msg.payload << 8) | b;
			m_state++;
			break;
		}
		if (m_state == STATE_DONE) {
			m_state = STATE_SYNC;
			return true;
		}
		return false;
	}
};

int main()
{
	Uart uart;

	DDRB = (1 << 7);
	DDRD = (1 << 5);

	sei();

	while (true) {
/*		char c;
		if (uart.can_get(1) && uart.can_put(1)) {
			if (uart.get(&c, 1) && uart.put(&c, 1) && c == '\r') {
				c = '\n';
			}
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
				c ^= 0x20;
			}
			uart.put(&c, 1);
			PORTD ^= (1 << 5);
		}*/
		if (uart.can_put(13)) {
			uart.put("Hello World\n\r", 13);
		}
		PORTB ^= (1 << 7);
	}

	/*	TCCR1A = 0;
	    TCCR1B = (1 << CS00);
	    DDRD = (1 << 5);

	    uint8_t a = 0;
	    uint8_t dir = 0;
	    uint16_t l = 0;
	    while (1) {
	        a++;
	        if (a == 4) {
	            a = 0;
	            if (l == 0 || l == 65535) {
	                dir = ~dir;
	            }
	            if (dir) {
	                l++;
	            } else {
	                l--;
	            }
	        }

	        PORTD = (TCNT1 < l) ? (1 << 5) : 0;
	    }*/
}
