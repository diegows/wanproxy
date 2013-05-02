
#ifndef	IO_NET_TCP_POLL_SERVER_H
#define	IO_NET_TCP_POLL_SERVER_H

#include <io/socket/socket.h>
#include <io/net/tcp_pool.h>

class TCPPoolServer {
	LogHandle log_;

	Socket *socket_;
	Action *read_notify_action_;
	Action *accept_action_;
	EventCallback *accept_callback_;

	TCPPoolServer(Socket *socket);
	~TCPPoolServer();

	void read_notify_callback(Event e);
	void accept_cancel(void);
	Action *accept_(EventCallback *);

public:
	static Action *accept(Socket *, EventCallback *);
};

#endif /* !IO_NET_TCP_SERVER_H */
