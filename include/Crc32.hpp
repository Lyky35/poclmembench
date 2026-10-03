#ifndef CRC32_HPP_
#define CRC32_HPP_

#include <cstddef>
#include <cstdint>

/**
 * Small table driven CRC-32 (IEEE 802.3 / ITU-T V.42, polynomial 0xEDB88320).
 * Used to verify the content of every benchmark chunk read back from the device.
 */
namespace crc32 {

namespace detail {
struct Table {
	uint32_t v[256];
	Table() {
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t c = i;
			for (int bit = 0; bit < 8; bit++)
				c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			v[i] = c;
		}
	}
};

inline const Table& table() {
	static const Table t;
	return t;
}
}

inline uint32_t init() {
	return 0xFFFFFFFFu;
}

inline uint32_t update(uint32_t crc, const void *data, size_t len) {
	const unsigned char *p = static_cast<const unsigned char*>(data);
	const uint32_t *t = detail::table().v;
	for (size_t i = 0; i < len; i++)
		crc = t[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
	return crc;
}

inline uint32_t finish(uint32_t crc) {
	return crc ^ 0xFFFFFFFFu;
}

inline uint32_t compute(const void *data, size_t len) {
	return finish(update(init(), data, len));
}

}

#endif /* CRC32_HPP_ */
