#include "avr/bus.hpp"

#include <iostream>
#include <queue>
#include <system_error>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

class Connection {
public:
	int fd_rd = -1;
	int fd_wr = -1;

	Connection(const char *dev)
	{
		fd_rd = open(dev, O_RDONLY | O_NONBLOCK);
		if (fd_rd < 0) {
			throw std::system_error(errno, std::system_category());
		}
		fd_wr = open(dev, O_WRONLY);
		if (fd_wr < 0) {
			throw std::system_error(errno, std::system_category());
		}
	}

	~Connection()
	{
		close(fd_rd);
		close(fd_wr);
	}

	std::queue<char> buf;

	Connection *src;

	explicit Connection(Connection *src = nullptr) : src(src) {}

	uint8_t get(char *c, uint8_t s)
	{
		can_get(s);
		uint8_t res = 0;
		while (s > 0 && !buf.empty()) {
			if (res == 0) {
				std::cout << "Received ";
			}
			*c = buf.front();
			buf.pop();

			std::cout << std::hex << (int)((uint8_t)*c);

			--s;
			++c;
			++res;
		}
		if (res > 0) {
			std::cout << std::endl;
		}
		return res;
	}

	bool can_get(uint8_t s)
	{
		while (true) {
			if (buf.size() >= s) {
				return true;
			}
			uint8_t b;
			int count = read(fd_rd, &b, 1); // Non-blocking call
			if (count <= 0) {
				return false;
			}
			buf.push(b);
		}
	}

	uint8_t put(char *c, uint8_t s)
	{
		std::cout << "Sent ";
		for (int i = 0; i < s; i++) {
			std::cout << std::hex << (int)((uint8_t)(c[i]));
		}
		std::cout << std::endl;
		int res = write(fd_wr, c, s);
		if (res != s) {
			throw std::system_error(errno, std::system_category());
		}
		return s;
	}

	bool can_put(uint8_t s) { return true; }
};

int main(int argc, char *argv[])
{
	if (argc != 4) {
		std::cout << "./led_ctrl_host TAR CMD PAYLOAD" << std::endl;
		return 1;
	}
	Connection connection("/dev/ttyUSB0");
	BusMaster bus;

	bus.msg().tar = std::stoi(argv[1], 0, 0);
	bus.msg().cmd = std::stoi(argv[2], 0, 0);
	bus.msg().payload = std::stoi(argv[3], 0, 0);
	bus.send();
	while (!bus.process(connection)) {
	}
}
