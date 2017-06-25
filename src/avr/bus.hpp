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

/**
 * @file message.hpp
 *
 * Message format used to send commands between controllers.
 *
 * @author Andreas Stöckel
 */

#pragma once

#include <stdint.h>

struct Message {
	static constexpr uint8_t CMD_NOP = 0x00;
	static constexpr uint8_t CMD_RAMP = 0x01;
	static constexpr uint8_t CMD_VAL = 0x02;
	static constexpr uint8_t CMD_ACK = 0xFF;

	uint8_t tar = 0;
	uint8_t cmd = 0;
	uint16_t payload = 0;
};

template <bool master>
struct Bus {
public:
	static constexpr uint16_t SYNC_WORD = 0xAFFE;
	static constexpr uint8_t READ_STATE_SYNC = 0;
	static constexpr uint8_t READ_STATE_TAR = 1;
	static constexpr uint8_t READ_STATE_CMD = 2;
	static constexpr uint8_t READ_STATE_PAYLOAD_H = 3;
	static constexpr uint8_t READ_STATE_PAYLOAD_L = 4;
	static constexpr uint8_t READ_STATE_DONE = 5;

	static constexpr uint8_t WRITE_STATE_SYNC_H = 0;
	static constexpr uint8_t WRITE_STATE_SYNC_L = 1;
	static constexpr uint8_t WRITE_STATE_TAR = 2;
	static constexpr uint8_t WRITE_STATE_CMD = 3;
	static constexpr uint8_t WRITE_STATE_PAYLOAD_H = 4;
	static constexpr uint8_t WRITE_STATE_PAYLOAD_L = 5;
	static constexpr uint8_t WRITE_STATE_DONE = 6;

private:
	uint8_t m_read_state = READ_STATE_SYNC;
	uint8_t m_write_state = WRITE_STATE_DONE;
	uint16_t m_sync = 0;
	bool m_has_msg = false;
	Message m_msg;

	bool handle_byte(uint8_t b)
	{
		switch (m_read_state) {
		case READ_STATE_SYNC:
			m_sync = (m_sync << 8) | b;
			if (m_sync == SYNC_WORD) {
				m_read_state++;
			}
			break;
		case READ_STATE_TAR:
			msg().tar = b;
			m_read_state++;
			break;
		case READ_STATE_CMD:
			msg().cmd = b;
			m_read_state++;
			break;
		case READ_STATE_PAYLOAD_H:
		case READ_STATE_PAYLOAD_L:
			msg().payload = (msg().payload << 8) | b;
			m_read_state++;
			break;
		}
		return m_read_state == READ_STATE_DONE;
	}

	uint8_t produce_byte()
	{
		switch (m_write_state) {
		case WRITE_STATE_SYNC_H:
			m_write_state++;
			return SYNC_WORD >> 8;
		case WRITE_STATE_SYNC_L:
			m_write_state++;
			return SYNC_WORD & 0xFF;
		case WRITE_STATE_TAR:
			m_write_state++;
			return master ? m_msg.tar : ((m_msg.tar == 255 || m_msg.tar == 0)
			                                 ? 255
			                                 : (m_msg.tar - 1));
		case WRITE_STATE_CMD:
			m_write_state++;
			return (m_msg.tar == 0 && !master) ? Message::CMD_ACK : m_msg.cmd;
		case WRITE_STATE_PAYLOAD_H:
			m_write_state++;
			return (m_msg.payload +
			        ((m_msg.cmd == Message::CMD_ACK) ? 1 : 0)) >>
			       8;
		case WRITE_STATE_PAYLOAD_L:
			m_write_state++;
			return (m_msg.payload + ((m_msg.cmd == Message::CMD_ACK) ? 1 : 0)) &
			       0xFF;
		default:
			return 0;
		}
	}

public:
	template <typename Uart>
	bool process(Uart &uart)
	{
		char c;
		m_has_msg = false;
		while (m_write_state != WRITE_STATE_DONE || !m_has_msg) {
			// Check whether there is data to send -- if yes, send it!
			if (m_write_state != WRITE_STATE_DONE) {
				if (uart.can_put(1)) {
					c = produce_byte();
					uart.put(&c, 1);
					continue;
				}
				break;
			}

			// Abort once there are no more bytes to read
			if (!uart.get(&c, 1)) {
				break;
			}

			// Handle the byte, if a message was read, switch to the "write"
			// state
			if (handle_byte(c)) {
				m_has_msg = master ? true : (m_msg.tar == 0);
				m_read_state = READ_STATE_SYNC;
				if (!master) {
					m_write_state = WRITE_STATE_SYNC_H;
				}
			}
		}
		return m_has_msg;
	}

	Message &msg() { return m_msg; }

	void send() { m_write_state = WRITE_STATE_SYNC_H; }
};

class BusClient : public Bus<false> {
};

class BusMaster : public Bus<true> {
};

