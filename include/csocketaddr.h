#pragma once
#include <arpa/inet.h>
#include <cstring>
#include <string>

class csocketaddr : private sockaddr_storage {
public:
	csocketaddr() { std::memset(this, 0, sizeof(sockaddr_storage)); }

	inline const struct sockaddr* sa() const { return (const struct sockaddr*)this; }
	inline struct sockaddr* sa() { return (struct sockaddr*)this; }

	inline socklen_t size() const {
		return ss_family == AF_INET ? sizeof(sockaddr_in) :
			(ss_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_storage));
	}

	inline sockaddr_storage* operator ->() { return this; }

	inline const sockaddr_in& in4() const { return *(const sockaddr_in*)this; }

	inline const csocketaddr& in4(uint32_t ip, uint16_t port = 0) {
		((sockaddr_in*)this)->sin_family = AF_INET;
		((sockaddr_in*)this)->sin_port = htons(port);
		((sockaddr_in*)this)->sin_addr.s_addr = (in_addr_t)htonl(ip);
		return *this;
	}
	inline const csocketaddr& in4(const std::string& ip, uint16_t port = 0) {
		((sockaddr_in*)this)->sin_family = AF_INET;
		((sockaddr_in*)this)->sin_port = htons(port);
		if (ip.empty()) {
			((sockaddr_in*)this)->sin_addr.s_addr = INADDR_ANY;
		}
		else {
			inet_pton(AF_INET, ip.c_str(), &((sockaddr_in*)this)->sin_addr.s_addr);
		}
		return *this;
	}

	inline const sockaddr_in6& in6() const { return *(sockaddr_in6*)this; }
	inline const csocketaddr& in6(uint8_t ip[16], uint16_t port = 0) {
		((sockaddr_in6*)this)->sin6_family = AF_INET6;
		((sockaddr_in6*)this)->sin6_port = htons(port);
		std::memcpy(((sockaddr_in6*)this)->sin6_addr.__in6_u.__u6_addr8, ip, 16);
		return *this;
	}
	inline csocketaddr& in6(const std::string& ip, uint16_t port = 0) {
		((sockaddr_in6*)this)->sin6_family = AF_INET6;
		((sockaddr_in6*)this)->sin6_port = htons(port);

		if (ip.empty()) {
			std::memset(((sockaddr_in6*)this)->sin6_addr.__in6_u.__u6_addr8, 0, 16);
		}
		else {
			inet_pton(AF_INET6, ip.c_str(), ((sockaddr_in6*)this)->sin6_addr.__in6_u.__u6_addr8);
		}

		return *this;
	}

	inline uint16_t port() {
		return htons(ss_family == AF_INET ? ((sockaddr_in*)this)->sin_port :
			(ss_family == AF_INET6 ? ((sockaddr_in6*)this)->sin6_port : 0));
	}

	inline std::string addr() {
		char address_buffer[INET6_ADDRSTRLEN];
		if (ss_family == AF_INET) {
			return inet_ntop(ss_family, &((sockaddr_in*)this)->sin_addr.s_addr, address_buffer, INET6_ADDRSTRLEN);
		}
		else if (ss_family == AF_INET6) {
			return inet_ntop(ss_family, ((sockaddr_in6*)this)->sin6_addr.__in6_u.__u6_addr8, address_buffer, INET6_ADDRSTRLEN);
		}
		return {};
	}

	inline uint16_t family() { return ss_family; }

	inline uint16_t ip() { return ss_family == AF_INET ? 4 : (ss_family == AF_INET6 ? 6 : 0); }
};
