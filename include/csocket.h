#pragma once
#include <memory>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cmath>
#include <linux/if_packet.h>
#include "csocketaddr.h"

class csocket {
	mutable std::shared_ptr<int> handle;
public:
	virtual ~csocket() { ; }
	csocket(int type, uint16_t proto, int family = AF_INET) 
		: handle(new int(-1), [](int* h) { ((*h) > 0) && ::close(*h); delete h; }) 
	{ 
		auto result = this->socket(type, proto, family);
		if (result != 0) {
			std::__throw_system_error(-result);
		}
	}
	csocket(int soc = -1) : handle(new int(soc), [](int* h) { ((*h) > 0) && ::close(*h); delete h; }) { ; }
	csocket(const csocket& h) : handle(h.handle) { ; }
	inline csocket& operator = (const csocket& h) { handle = h.handle; return *this; }
	inline const int& h() const { return *handle; }
public:
	/*
	* Create new socket handle
	*/
	inline ssize_t socket(int type, uint16_t proto, int family = AF_INET) { 
		close(); 
		*handle = ::socket(family, type, proto);
		return *handle != -1 ? 0 : -errno;
	}

	/*
	* Close socket handle
	*/

	inline void close() { if ((*handle > 0)) { ::close(*handle); *handle = -1; } }

	/*
	* Attach raw socket handle
	*/
	inline void attach(int soc) { *handle = soc; }

	/*
	* Detach raw socket handle
	*/
	inline int detach() { int soc = *handle; *handle = -1; return soc; }

	/*
	* Set socket option value
	*/
	template<typename T>
	inline ssize_t setoption(int level, int optname, const T& value) const { return setoption(level, optname, (const void*)& value, sizeof(T)); }
	
	/*
	* Set socket option value
	*/
	inline ssize_t setoption(int level, int optname, const void* value, size_t length) const {
		return !::setsockopt(*handle, level, optname, value, (socklen_t)length) ? 0 : -errno;
	}

	/*
	* Get socket option value
	*/
	template<typename T>
	inline ssize_t getoption(int level, int optname, const T& value) const { size_t optlength = sizeof(T); return getoption(level, optname, (const void*)& value, optlength); }

	/*
	* Get socket option value
	*/
	inline ssize_t getoption(int level, int optname, void* value, size_t& length) const {
		return !::getsockopt(*handle, level, optname, value, (socklen_t*)&length) ? 0 : -errno;
	}

	/*
	* Receive data from udp socket
	*/
	inline ssize_t recvfrom(void* buffer, size_t length, csocketaddr& ainfo, int flags = 0) const {
		socklen_t len = sizeof(csocketaddr);
		auto recvd = ::recvfrom(*handle, buffer, length, flags, ainfo.sa(), &len);
		return recvd >= 0 ? recvd : -errno;
	}


	/*
	* Receive data from udp socket with original destination address (use for tproxy mode)
	*/
	inline ssize_t recvfrom(void* buffer, size_t length, csocketaddr& asrc, csocketaddr& adst, int flags = 0) const {
		struct msghdr           msg;
		char                    cntrlbuf[4096];
		struct iovec            iov[1];
		//	struct sockaddr_in      clntaddr;
		msg.msg_name = asrc.sa();
		msg.msg_namelen = asrc.size();
		msg.msg_control = cntrlbuf;
		msg.msg_controllen = sizeof(cntrlbuf);
		iov[0].iov_base = buffer;
		iov[0].iov_len = length;
		msg.msg_iov = iov;
		msg.msg_iovlen = 1;
		ssize_t ret = ::recvmsg(*handle, &msg, flags);
		if (ret <= 0)
		{
			return -errno;
		}

		msg.msg_iov[0].iov_len = (size_t)ret;

		{
			struct cmsghdr* cmsg;
			adst->ss_family = 0;
			for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
			{
				if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVORIGDSTADDR)
				{
					memcpy(&adst, CMSG_DATA(cmsg), sizeof(struct sockaddr_in));
					adst->ss_family = AF_INET;
					break;
				}
			}
		}
		return ret;
	}
	inline ssize_t sendto(const void* buffer, size_t length, const csocketaddr& ainfo, int flags = 0) const {
		auto result = ::sendto(*handle, buffer, length, flags, ainfo.sa(), ainfo.size());
		return result >= 0 ? result : -errno;
	}

	/*
	* Check the data available length
	*/
	inline ssize_t available() const {
		ulong sz = 0;
		return ioctl(*handle, FIONREAD, &sz) == 0 ? sz : 0;
	}

	/*
	* Receive data from tcp socket
	*/
	inline ssize_t recv(void* buffer, size_t length,int flags = 0) const {
		auto len = ::recv(*handle, buffer, length, flags);
		return len >= 0 ? len : -errno;
	}

	/*
	* Send data to tcp socket
	*/
	inline ssize_t send(const void* buffer, size_t length, int flags = 0) const {
		auto len = ::send(*handle, buffer, length, flags);
		return len >= 0 ? len : -errno;
	}

	/*
	* Accept new tcp connection
	*/
	inline ssize_t accept(csocket& so) const { csocketaddr ainfo; return accept(so, ainfo); }
	
	/*
	* Accept new tcp connection
	*/
	inline ssize_t accept(csocket& so, csocketaddr& ainfo) const {
		socklen_t linfo = sizeof(csocketaddr);
		so = csocket(::accept(*handle, ainfo.sa(), &linfo));
		return *so.handle > 0 ? 0 : -errno;
	}

	inline ssize_t connect();

	/*
	* Listen tcp socket for connection
	*/
	inline ssize_t listen(size_t port, size_t maxcon = SOMAXCONN) const {
		return ::listen(*handle, (int)maxcon) == 0 ? 0 : -errno;
	}

	/*
	* Bind to specific address
	*/
	inline ssize_t bind(const csocketaddr& ainfo) const {
		return ::bind(*handle, ainfo.sa(), ainfo.size()) == 0 ? 0 : -errno;
	}

	/*
	* Bind to specific ip4 address
	*/
	inline ssize_t csocket::bind4(uint16_t port, uint32_t ip = INADDR_ANY) const {
		csocketaddr ainfo;
		return bind(ainfo.in4(ip, port));
	}
	/*
	* Bind to specific ip4 address
	*/
	inline ssize_t csocket::bind4(uint16_t port, const std::string& ip) const {
		csocketaddr ainfo;
		return bind(ainfo.in4(ip, port));
	}
	/*
	* Bind to specific ip6 address
	*/
	inline ssize_t csocket::bind6(uint16_t port, uint8_t* ip) const {
		csocketaddr ainfo;
		return bind(ainfo.in6(ip, port));
	}
	/*
	* Bind to specific ip6 address
	*/
	inline ssize_t csocket::bind6(uint16_t port, const std::string& ip) const {
		csocketaddr ainfo;
		return bind(ainfo.in6(ip, port));
	}

	/*
	* Bind to specific interface
	*/
	inline ssize_t csocket::bind_iface(const std::string& eth_device) const {
		if (!eth_device.empty()) {
			return setoption(SOL_SOCKET, SO_BINDTODEVICE, eth_device.data(), eth_device.length());
		}
		return -EADDRNOTAVAIL;
	}

	/*
	* Set socket timeout
	*/
	inline ssize_t settimeout(csocket& soc, size_t milliseconds, int optname = SO_RCVTIMEO) {
		struct timeval timeout;
		timeout.tv_sec = (time_t)std::floor(milliseconds / 1000);
		timeout.tv_usec = (milliseconds % 1000) * 1000;
		return soc.setoption(SOL_SOCKET, optname, &timeout, sizeof(timeout));
	}

	/*
	* Enablle Transparent proxy mode
	* recvorigdstaddr option is enabled receive original dest address (@see recvfrom(void* buffer, size_t length, csocketaddr& asrc, csocketaddr& adst, int flags = 0))
	*/
	inline ssize_t settproxymode(bool recvorigdstaddr = true) {
		auto result = setoption(SOL_IP, IP_TRANSPARENT, 1);
		return result == 0 && recvorigdstaddr ? setoption(SOL_IP, IP_RECVORIGDSTADDR, 1) : result;
	}

	/*
	* Enablle reuse binding address
	*/
	inline ssize_t setreuseaddr() {
		return setoption(SOL_SOCKET, SO_REUSEADDR, 1);
	}

	/*
	* Enablle reuse binding port
	*/
	inline ssize_t setreuseport() {
		return setoption(SOL_SOCKET, SO_REUSEPORT, 1);
	}

	/*
	* Enable receive IP header
	*/
	inline ssize_t setrecviphdr() {
		return setoption(IPPROTO_IP, IP_HDRINCL, 1);
	}

	/*
	* Set socket fanout mode
	*/
	inline ssize_t setfanoutmode(uint16_t group, int type = PACKET_FANOUT_LB,int pid = -1) {
		(pid == -1) && (pid = getpid());
		int fanout_id = (((int)group << 11) | (pid & 0x07ff));
		int fanout_opt = int(fanout_id | (type << 16));
		return setoption(SOL_PACKET, PACKET_FANOUT, fanout_opt);
	}

	/*
	* Set tcp no delay
	*/
	inline ssize_t settcpnodelay() {
		return setoption(SOL_TCP, TCP_NODELAY, 1);
	}
	/*
	* Set tcp fast open connection
	*/
	inline ssize_t settcpfastopen() {
		return setoption(SOL_TCP, TCP_FASTOPEN, 1);
	}
	
	/** 
	* Enable KeepAlive tcp connection
	* int keepintvl -	The time (in seconds) between individual keepalive probes. (TCP_KEEPINTVL socket option)
	* int keepidle -	The time (in seconds) the connection needs to remain
	*					idle before TCP starts sending keepalive probes (TCP_KEEPIDLE socket option)
	* int keepcnt -		The maximum number of keepalive probes TCP should
	*					send before dropping the connection. (TCP_KEEPCNT socket option)
	*/

	inline ssize_t settcpkeepalive(int keepintvl = -1, int keepidle = -1, int keepcnt = -1) {
		auto result = setoption(SOL_SOCKET, SO_KEEPALIVE, 1);
		(result == 0 && keepcnt != -1 && (result = setoption(IPPROTO_TCP, TCP_KEEPCNT, keepcnt)));
		(result == 0 && keepidle != -1 && (result = setoption(IPPROTO_TCP, TCP_KEEPIDLE, keepidle)));
		(result == 0 && keepintvl != -1 && (result = setoption(IPPROTO_TCP, TCP_KEEPINTVL, keepintvl)));
		return result;
	}

	/*
	* Set block\unblock mode
	*/
	inline ssize_t setblockmode(bool block = true) const {
		ulong mode = ulong(block);
		return ioctl(*handle, FIONBIO, &mode) == 0 ? 0 : -errno;
	}
};

namespace std {
	template <>
	struct hash<csocket>
	{
		inline size_t operator()(const csocket& v) const { return v.h(); }
	};

	template <>
	struct equal_to<csocket>
	{
		inline bool operator()(const csocket& a, const csocket& b) const {
			return a.h() == b.h();
		}
	};
}