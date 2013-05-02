
#include <event/event_callback.h>

#include <io/socket/socket.h>

#include <io/net/tcp_pool_server.h>


TCPPoolServer::TCPPoolServer(Socket *socket)
: log_("/tcp_pool/server"),
  socket_(socket),
  read_notify_action_(NULL),
  accept_action_(NULL),
  accept_callback_(NULL)
{ 
        
}

TCPPoolServer::~TCPPoolServer()
{

	ASSERT(log_, read_notify_action_ == NULL);
	ASSERT(log_, accept_action_ == NULL);
	ASSERT(log_, accept_callback_ == NULL);

}

Action *
TCPPoolServer::accept(Socket *socket, EventCallback *accept_callback)
{

	TCPPoolServer *tcp_pool_server = new TCPPoolServer(socket);

	return tcp_pool_server->accept_(accept_callback);

}

Action *
TCPPoolServer::accept_(EventCallback *accept_callback)
{

	EventCallback *cb = callback(this,
				&TCPPoolServer::read_notify_callback);
	read_notify_action_ = socket_->read_notify(cb);
	accept_callback_ = accept_callback;

	return cancellation(this, &TCPPoolServer::accept_cancel);

}

void
TCPPoolServer::read_notify_callback(Event e)
{

	ASSERT(log_, read_notify_action_ != NULL);
	ASSERT(log_, accept_callback_ != NULL);

	read_notify_action_->cancel();
	read_notify_action_ = NULL;

	switch (e.type_) {
        case Event::Done:
		accept_callback_->param(e);
		accept_action_ = accept_callback_->schedule();
		accept_callback_ = NULL;
                break;
        case Event::EOS:
        case Event::Error:
                INFO(log_) << "Poll returned error: " << e;
		break;
        default:
                HALT("Listener") << "Unexpected event: " << e;
		break;
        }

}

void 
TCPPoolServer::accept_cancel(void)
{

	ASSERT(log_, (read_notify_action_ != NULL || accept_action_ != NULL));

	if (read_notify_action_ != NULL) {
		read_notify_action_->cancel();
		read_notify_action_ = NULL;
	}
	else if (accept_action_ != NULL) {
		accept_action_->cancel();
		accept_action_ = NULL;
	}
		
	delete this;

}

