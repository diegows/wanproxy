#ifndef	IO_NET_TCP_POLL_CLIENT_H
#define	IO_NET_TCP_POLL_CLIENT_H

#include <io/socket/socket.h>
#include <deque>
#include <map>
#include <io/net/tcp_pool.h>

#define MAX_CONNECTING 30
#define MIN_CONNECTED 30

class TCPClientSocketPool {
	LogHandle log_;
	std::deque<Socket *> socket_q;
	std::deque<SocketEventCallback *> callback_q;
	std::map<SocketEventCallback *, Action *> callback_act_map;
	std::map<Socket *, SocketPoolAction *> socket_keepalive_map;
	uint32_t connecting;

	SocketAddressFamily family;
	std::string iface;
	std::string name;

	void connect_complete(Event, SocketPoolAction *);
	void close_complete(SocketPoolAction *);
	void connect_cancel(SocketEventCallback *);
	void read_callback(Event, SocketPoolAction *);
	void delete_socket(Socket *socket);

	void mantain_sockets(void);

public:
	~TCPClientSocketPool(void);
	TCPClientSocketPool(SocketAddressFamily, const std::string&);
	TCPClientSocketPool(SocketAddressFamily, const std::string&, const std::string&);
	Action *get_socket(SocketEventCallback *);
};

class TCPPoolClient {
	LogHandle log_;

	Action *close_action_;

	Action *connect_action_;
	SocketEventCallback *connect_callback_;

	TCPPoolClient();
	~TCPPoolClient();

	Action *connect_(TCPClientSocketPool *, SocketEventCallback *);
	void connect_cancel(void);
	void connect_complete(Event, Socket *);

	void close_complete(void);

public:
	static Action *connect(TCPClientSocketPool *, SocketEventCallback *);
};

#endif /* !IO_NET_TCP_CLIENT_H */
