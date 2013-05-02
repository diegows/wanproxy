#include <event/event_callback.h>

#include <io/socket/socket.h>

#include <io/net/tcp_pool_client.h>

#include <cstdlib>

TCPClientSocketPool::TCPClientSocketPool(SocketAddressFamily family_, 
                        const std::string &name_) :
                                log_("tcp_pool/client"),
                                socket_q(),
                                callback_q(),
                                callback_act_map(),
                                socket_keepalive_map(),
                                connecting(0),
                                family(family_),
                                iface(),
                                name(name_)
{

        mantain_sockets();

}

TCPClientSocketPool::TCPClientSocketPool(SocketAddressFamily family_, 
                        const std::string &iface_,
                        const std::string &name_) :
                                log_("tcp_pool/client"),
                                socket_q(),
                                callback_q(),
                                callback_act_map(),
                                socket_keepalive_map(),
                                connecting(0),
                                family(family_),
                                iface(iface_),
                                name(name_)
{

        mantain_sockets();

}

TCPClientSocketPool::~TCPClientSocketPool(void)
{

        while (!socket_q.empty()) {
                Socket *socket;

                socket = socket_q.front();
                socket_q.pop_front();

                delete_socket(socket);
        }

}

void
TCPClientSocketPool::close_complete(SocketPoolAction *socket_action)
{

        Socket *socket = socket_action->first;
        Action *action = socket_action->second;

        action->cancel();
        delete socket;

}

void 
TCPClientSocketPool::mantain_sockets(void)
{

        int connect;
        int i;

        if (callback_q.size() > 0)
                connect = callback_q.size() + MIN_CONNECTED - connecting;
        else
                connect = (MIN_CONNECTED - socket_q.size() - connecting);

        for(i = 0; i < connect; i++) {
                Socket *sock;
                sock = Socket::create(family, SocketTypeStream, "tcp", name);
                if (sock == NULL) {
                        WARNING(log_) << "Socket creation failed!";

                        return;
                }

                sock->set_keepalive(10, 10, 3);
                if (iface != "" && !sock->bind(iface)) {
                        /*
                         * XXX
                         * I think maybe just pass the Socket up to the caller
                         * and make them close it?
                         *
                         * NB:
                         * Not duplicating this to other similar code.
                         */

                        HALT(log_) << "Socket bind failed.";
                        exit(0);

                        return;
                }

                SocketPoolAction *socket_action = new SocketPoolAction();
                socket_action->first = sock;
                EventCallback *cb = callback(this, 
                                        &TCPClientSocketPool::connect_complete,
                                        socket_action);
                socket_action->second = sock->connect(name, cb);
                connecting++;

        }

}

void
TCPClientSocketPool::connect_complete(Event e, SocketPoolAction *socket_action)
{

        Socket *socket = socket_action->first;
        Action *connect_action = socket_action->second;

        connect_action->cancel();

        connecting--;

        if (e.type_ != Event::Done) {
                WARNING(log_) << "Socket connection failed!";
                delete_socket(socket);
                return;
        }
        
        if (callback_q.empty()) {
                socket_q.push_back(socket);

                SocketPoolAction *read_socket_action = new SocketPoolAction();
                read_socket_action->first = socket;
                EventCallback *cb = callback(this, 
                                        &TCPClientSocketPool::read_callback,
                                        read_socket_action);
                read_socket_action->second = socket->read_notify(cb);
                socket_keepalive_map[socket] = read_socket_action;
        }
        else {
                SocketEventCallback *connect_callback;

                connect_callback = callback_q.front();
                callback_q.pop_front();

                connect_callback->param(e, socket);
                Action *action = connect_callback->schedule();
                callback_act_map[connect_callback] = action;
        }

        delete socket_action;

}

void
TCPClientSocketPool::read_callback(Event e, SocketPoolAction *socket_action)
{

        Socket *socket = socket_action->first;
        int ret;
        char buf[64];

        if (e.type_ != Event::Done) {
                WARNING(log_) << "Socket read failed!";
                delete_socket(socket);
                return;
        }

        ret = socket->raw_read(buf, sizeof(buf));
        if (ret > 0) {
                WARNING(log_) << "Socket has data to read from server!";
                WARNING(log_) << "Not supported by TCP POOL!";
                return;
        }

        WARNING(log_) << "Socket connection lost!";
        delete_socket(socket);

}

Action *
TCPClientSocketPool::get_socket(SocketEventCallback *ccb)
{

        if (!socket_q.empty()) {
                Socket *socket;

                socket = socket_q.front();
                socket_q.pop_front();
        
                delete_socket(socket);
                
                ccb->param(Event::Done, socket);
                Action *action = ccb->schedule();
                callback_act_map[ccb] = action;
        }
        else {
                callback_q.push_back(ccb);
        }

        mantain_sockets();

        return (cancellation(this, &TCPClientSocketPool::connect_cancel, ccb));

}

void 
TCPClientSocketPool::connect_cancel(SocketEventCallback *ccb)
{

        std::deque<SocketEventCallback *>::iterator cb_it;
        Action *action = callback_act_map[ccb];

        cb_it = callback_q.begin();

        while (cb_it != callback_q.end()) {
                if (*cb_it == ccb) {
                        callback_q.erase(cb_it);
                        action = dynamic_cast<Action *>(*cb_it);
                        action->cancel();
                        delete *cb_it;
                        return;
                }
                cb_it++;
        }
        
        ASSERT(log_, callback_act_map.find(ccb) != callback_act_map.end());
        action = callback_act_map[ccb];
        action->cancel();
        callback_act_map.erase(ccb);
                
}

/*
 * We delete the socket object only if we own it (if it exists in the queue).
 */
void
TCPClientSocketPool::delete_socket(Socket *socket)
{
        std::deque<Socket *>::iterator sock_it;
        SocketPoolAction *read_socket_action;
        Action *action;

        read_socket_action = socket_keepalive_map[socket];
        action = read_socket_action->second;
        action->cancel();
        socket_keepalive_map.erase(socket);
        delete read_socket_action;

        sock_it = socket_q.begin();

        while (sock_it != socket_q.end()) {
                if (*sock_it == socket) {
                        socket_q.erase(sock_it);
                        SocketPoolAction *socket_action =
                                                new SocketPoolAction();
                        socket_action->first = socket;
                        SimpleCallback *cb = callback(this,
                                        &TCPClientSocketPool::close_complete,
                                        socket_action);
                        socket_action->second = socket->close(cb);

                        return;
                }
                sock_it++;
        }

} 

TCPPoolClient::TCPPoolClient()
: log_("/tcp/client"),
  close_action_(NULL),
  connect_action_(NULL),
  connect_callback_(NULL)
{ 
        
}

TCPPoolClient::~TCPPoolClient()
{

	ASSERT(log_, connect_action_ == NULL);
	ASSERT(log_, connect_callback_ == NULL);
	ASSERT(log_, close_action_ == NULL);

}

Action *
TCPPoolClient::connect_(TCPClientSocketPool *socket_pool, 
                                SocketEventCallback *ccb)
{
	ASSERT(log_, connect_action_ == NULL);
	ASSERT(log_, connect_callback_ == NULL);

        SocketEventCallback *cb = callback(this, 
                                &TCPPoolClient::connect_complete);
        connect_action_ = socket_pool->get_socket(cb);
        connect_callback_ = ccb;

	return (cancellation(this, &TCPPoolClient::connect_cancel));
}

void
TCPPoolClient::connect_cancel(void)
{

	ASSERT(log_, close_action_ == NULL);
	ASSERT(log_, connect_action_ != NULL);

	connect_action_->cancel();
	connect_action_ = NULL;

	if (connect_callback_ != NULL) {
		delete connect_callback_;
		connect_callback_ = NULL;
	} else {
		delete this;
		return;
	}

}

void
TCPPoolClient::connect_complete(Event e, Socket *sock)
{

	connect_action_->cancel();
	connect_action_ = NULL;

        connect_callback_->param(e, sock); 
        connect_action_ = connect_callback_->schedule();
        connect_callback_ = NULL;

}

void
TCPPoolClient::close_complete(void)
{
	close_action_->cancel();
	close_action_ = NULL;

	delete this;
}

Action *
TCPPoolClient::connect(TCPClientSocketPool *socket_pool, 
                                SocketEventCallback *ccb)
{
	TCPPoolClient *tcp = new TCPPoolClient();
	return (tcp->connect_(socket_pool, ccb));
}


