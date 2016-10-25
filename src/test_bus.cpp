#include <iostream>
#include <queue>
#include <sstream>
#include <string>

#include "avr/bus.hpp"

class Connection {
public:
	std::queue<char> buf;

	Connection *src;

	explicit Connection(Connection *src = nullptr) : src(src) {}

	uint8_t get(char *c, uint8_t s)
	{
		std::queue<char> &q = src->buf;
		uint8_t res = 0;
		while (s > 0 && !q.empty()) {
			*c = q.front();
			q.pop();
			--s;
			++c;
			++res;
		}
		return res;
	}

	bool can_get(uint8_t s) { return src->buf.size() >= s; }

	uint8_t put(char *c, uint8_t s)
	{
		uint8_t res = 0;
		while (s > 0 && buf.size() < 16) {
			buf.push(*c);
			--s;
			++c;
			++res;
		}
		return res;
	}

	bool can_put(uint8_t s) { return (15 - buf.size()) >= s; }
};

static std::string format_msg(Message msg)
{
	std::stringstream ss;
	switch (msg.cmd) {
	case Message::CMD_NOP:
		ss << "NOP with payload " << msg.payload;
		break;
	case Message::CMD_ACK:
		ss << "ACK with payload " << msg.payload;
		break;
	case Message::CMD_RAMP:
		ss << "RAMP for channel " << (msg.payload >> 12) << " with value "
		   << (msg.payload & 0x0FFF);
		break;
	case Message::CMD_VAL:
		ss << "VAL for channel " << (msg.payload >> 12) << " with value "
		   << (msg.payload & 0x0FFF);
		break;
	}
	return ss.str();
};

int main()
{
	// Once master, three clients
	BusMaster m;
	BusClient c1, c2, c3;

	// Setup a ring connection m -> c1 -> c2 -> c3 -> m
	Connection m_c1;
	Connection c1_c2(&m_c1);
	Connection c2_c3(&c1_c2);
	Connection c3_m(&c2_c3);
	m_c1.src = &c3_m;

	auto process_and_display_message = [](const std::string &name, auto &bus,
	                                      auto &connection) {
		if (bus.process(connection)) {
			std::cout << "\'" << name << "\' received message "
			          << format_msg(bus.msg());
			if (bus.msg().tar == 255) {
				std::cout << " for master";
			} else {
				std::cout << " for client " << (int)bus.msg().tar;
			}
			std::cout << std::endl;
		}
	};

	auto run = [&] {
		do {
			process_and_display_message("m", m, m_c1);
			process_and_display_message("c1", c1, c1_c2);
			process_and_display_message("c2", c2, c2_c3);
			process_and_display_message("c3", c3, c3_m);
		} while (!m_c1.buf.empty() || !c1_c2.buf.empty() ||
		         !c2_c3.buf.empty() || !c3_m.buf.empty());
	};

	auto send_and_run = [&] (Message msg){
		m.msg() = msg;
		m.send();
		run();
	};

	send_and_run({0, Message::CMD_NOP, 0});
	send_and_run({1, Message::CMD_NOP, 0});
	send_and_run({2, Message::CMD_NOP, 0});
	send_and_run({3, Message::CMD_NOP, 0});
	send_and_run({4, Message::CMD_NOP, 0});

	send_and_run({0, Message::CMD_RAMP, 0x4002});

	return 0;
}
