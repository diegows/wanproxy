#include <common/test.h>

#include <event/event_callback.h>
#include <event/event_main.h>

#include <io/net/tcp_pool_client.h>
#include <io/net/tcp_server.h>

#include <io/socket/socket.h>

#include <iostream>

int conn;

class TestC {
        TCPClientSocketPool *socket_pool;
        Action *action;
        Action *close_action;
        Socket *socket;

        void connected(Event e, Socket *sock)
        {

                conn++;
                action->cancel();

                ASSERT("TestC", sock != NULL);
                if (e.type_ != Event::Done) {
                        DEBUG("connected") << "Connect failed.";
                        exit(-1);
                }

                EventCallback *cb = callback(this, &TestC::write_complete);
                std::string data = "diego\n";
                Buffer buf((uint8_t *)data.c_str(), data.size());
                action = sock->write(&buf, cb);
                socket = sock;

        }

        void write_complete(Event e)
        {
                
                ASSERT("TestC", e.type_ == Event::Done);
                action->cancel();

                EventCallback *cb = callback(this, &TestC::client_read);
                action = socket->read(0, cb);

        }

        void client_read(Event e)
        {

                ASSERT("TestC", e.type_ == Event::Done);
                action->cancel();
                std::string reply;

                e.buffer_.extract(reply);

                std::cout << reply << std::endl;

        }

        void close_event(void) {
                close_action->cancel();
                close_action = NULL;
                delete socket;
                delete this;
        }

public:
        ~TestC(void) 
        {

        }

        void main(TCPClientSocketPool *sock_pool)
        {

                SocketEventCallback *cb = callback(this, &TestC::connected);

                socket_pool = sock_pool;

                action = TCPPoolClient::connect(socket_pool, cb);

        }

        void shutdown(void)
        {
                SimpleCallback *cb = callback(this, 
                        &TestC::close_event);
                //DEBUG("socket") << socket;
                close_action = socket->close(cb);
        }
};

int main(void) {

        TCPClientSocketPool *socket_pool;
        std::deque<TestC *> testc_q;
        std::deque<TestC *>::iterator testc_it;

        conn = 0;

        socket_pool = new TCPClientSocketPool(SocketAddressFamilyIP,
                                                        "localhost:7");
        for(int i = 0; i < 100; i++) {
                TestC *t = new TestC;
                testc_q.push_back(t);
                t->main(socket_pool);
        }

        event_main();

        while (!testc_q.empty()) {
                TestC *t;

                t = testc_q.front();
                testc_q.pop_front();

                t->shutdown();
        }

        delete socket_pool;

        event_main();

        return 0;

}

