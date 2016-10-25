#include <cstdint>
#include <iostream>

		struct Buffer {
			uint8_t buf[16];
			uint8_t rd : 4;
			uint8_t wr : 4;

			Buffer() : rd(0), wr(0) {}

			uint8_t put(const char *b, uint8_t size)
			{
				uint8_t res = size;
				while ((wr + 1) != rd && res) {
					buf[wr] = *b;
					b++;
					wr++;
					res--;
				}
				return size - res;
			}

			uint8_t get(char *b, uint8_t size)
			{
				uint8_t res = size;
				while (rd != wr && res) {
					*b = buf[rd];
					b++;
					rd++;
					res--;
				}
				return size - res;
			}

			uint8_t wr_avail() const { return (wr - rd + 15) & 0x0F; }

			uint8_t rd_avail() const { return (rd - wr) & 0x0F; }
		};


int main() {

Buffer buf;
char c;

std::cout << (int)buf.wr_avail() << std::endl;
std::cout << (int)buf.rd_avail() << std::endl;
std::cout << (int)buf.put("Hallo Welt\n\r", 13) << std::endl;
std::cout << (int)buf.wr_avail() << std::endl;
std::cout << (int)buf.rd_avail() << std::endl;

while (buf.get(&c, 1));

std::cout << (int)buf.wr_avail() << std::endl;
std::cout << (int)buf.rd_avail() << std::endl;
std::cout << (int)buf.put("Hallo Welt\n\r", 13) << std::endl;
std::cout << (int)buf.wr_avail() << std::endl;
std::cout << (int)buf.rd_avail() << std::endl;

while (buf.get(&c, 1));

std::cout << (int)buf.wr_avail() << std::endl;
std::cout << (int)buf.rd_avail() << std::endl;


}
