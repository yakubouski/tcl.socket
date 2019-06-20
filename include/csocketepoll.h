#pragma once
#include "csocket.h"
#include <vector>
#include <sys/epoll.h>
#include <unordered_map>
#include <functional>

class csocketepoll {
	std::function<void(const csocket& so)>		_fnaccept;
	std::function<void(const csocket& so)>		_fnin;
	std::function<void(const csocket& so)>		_fndisconn;
	std::function<void(const csocket& so)>		_fnout;
	std::function<void(const csocket& so)>			_fnerr;
	std::vector<pollfd>								_pollfd;
	std::unordered_map<int, std::tuple<csocket, bool, short>>		_socks;
public:
	csocketepoll(
		const std::function<void(const csocket& so)>& pollaccept,
		const std::function<void(const csocket& so)>& pollin = {},
		const std::function<void(const csocket& so)>& polldisconn = {},
		const std::function<void(const csocket& so)>& pollout = {},
		const std::function<void(const csocket& so)>& pollerr = {}) :
		_fnaccept(pollaccept), _fnin(pollin), _fndisconn(polldisconn), _fnout(pollout), _fnerr(pollerr)
	{
		;
	}
	~csocketepoll() { ; }

	csocketepoll(const csocketepoll&) = delete;
	csocketepoll(const csocketepoll&&) = delete;
	csocketepoll& operator = (const csocketepoll&) = delete;
	csocketepoll& operator = (const csocketepoll&&) = delete;

	inline bool insert_server(const csocket& so, short poll_events = POLLIN | POLLERR) {
		return _socks.emplace(so.h(), std::make_tuple(so, true, poll_events)).second;
	}
	inline bool insert_client(const csocket& so, short poll_events = POLLIN | POLLERR | POLLHUP) {
		return _socks.emplace(so.h(), std::make_tuple(so, false, poll_events)).second;
	}
	inline size_t remove(const csocket& so) {
		return _socks.erase(so.h());
	}
	inline ssize_t listen(size_t miliseconds) {
		if (!_socks.empty()) {
			if (_socks.size() != _pollfd.size()) {
				_pollfd.resize(_socks.size());
				auto&& p_it = _pollfd.begin();
				for (auto&& so : _socks) {
					p_it->fd = so.first;
					p_it->events = POLLERR | POLLNVAL | std::get<2>(so.second);
					p_it->revents = 0;
					++p_it;
				}
			}
			auto&& wpoll = _pollfd.data();
			int result = ::poll(wpoll, _pollfd.size(), (int)miliseconds), events = result;
			if (result > 0) {
				for (size_t s = 0; result && s < _pollfd.size(); s++)
				{
					if (wpoll[s].revents) {
						--result;
						{
							auto&& soh = _socks.find(wpoll[s].fd);
							if (soh != _socks.end()) {
								if (wpoll[s].revents & POLLIN) {
									if (std::get<1>(soh->second)) {
										if (_fnaccept) { _fnaccept(std::get<0>(soh->second)); }
									}
									else if (_fnin) { _fnin(std::get<0>(soh->second)); }
								}
								if (wpoll[s].revents & POLLOUT) {
									if (_fnout) { _fnout(std::get<0>(soh->second)); }
								}
								if (wpoll[s].revents & POLLHUP) {
									if (_fndisconn) { _fndisconn(std::get<0>(soh->second)); }
									_socks.erase(wpoll[s].fd);
								}
								if (wpoll[s].revents & POLLERR) {
									if (_fnerr) { _fnerr(std::get<0>(soh->second)); }
									_socks.erase(wpoll[s].fd);
								}
								wpoll[s].revents = 0;
							}
							else {
								_socks.erase(wpoll[s].fd);
							}
						}
					}
				}
			}
			return events >= 0 ? events : (errno == EINTR ? 0 : -errno);
		}
		return -1;
	}
};