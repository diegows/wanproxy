/*
 * Copyright (c) 2008-2011 Juli Mallett. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <common/endian.h>

#include <event/event_callback.h>
#include <event/event_system.h>

#include <io/socket/socket.h>

#include <io/net/tcp_server.h>
#include <io/net/tcp_pool_server.h>

#include "proxy_connector.h"
#include "proxy_listener.h"

#include "wanproxy_codec_pipe_pair.h"

ProxyListener::ProxyListener(const std::string& name,
			     WANProxyCodec *interface_codec,
			     WANProxyCodec *remote_codec,
			     SocketAddressFamily interface_family,
			     const std::string& interface,
			     SocketAddressFamily remote_family,
			     const std::string& remote_name,
                                ConfigProto interface_proto,
                                ConfigProto peer_proto)
: SimpleServer<TCPServer>("/wanproxy/proxy/" + name + "/listener", interface_family, interface),
  name_(name),
  interface_codec_(interface_codec),
  remote_codec_(remote_codec),
  remote_family_(remote_family),
  socket_pool(NULL),
  remote_name_(remote_name),
  interface_proto_(interface_proto),
  peer_proto_(peer_proto)
{ 

        //INICIALIZAR SOCKET POOL ACA! 
        //Luego pasarlo al ProxyConnect sobrecargando al constructor.
        INFO("interface") << interface;
        INFO("name") << name;
        INFO("interface_family") << interface_family;

        if (peer_proto == ConfigProtoTCPPool) {
                socket_pool = new TCPClientSocketPool(remote_family,
                                                        remote_name);
        }
        
}

ProxyListener::~ProxyListener()
{ 

        if (socket_pool != NULL)
                delete socket_pool;
        
}

void
ProxyListener::client_connected(Socket *socket)
{

        switch (interface_proto_) {
        case ConfigProtoTCP:
                start_connector(socket);
                break;
        case ConfigProtoTCPPool: {
                SocketPoolAction *sock_action = new SocketPoolAction();
                sock_action->first = socket;
                EventCallback *cb = callback(this, 
                                        &ProxyListener::accept_callback,
                                        sock_action);
                Action *tcp_pool_action = TCPPoolServer::accept(socket, cb);
                sock_action->second = tcp_pool_action;
                break;
        }
        default:
                HALT("ProxyListenner") << "proto not supported";
        }

}

void
ProxyListener::accept_callback(Event e, SocketPoolAction *socket_action)
{
        
        Socket *socket;
        Action *action;

        ASSERT("/wanproxy/proxy_listener", e.type_ == Event::Done);

        socket = socket_action->first;
        action = socket_action->second;

        action->cancel();

        delete socket_action;
        
        start_connector(socket); 
        
}

void
ProxyListener::start_connector(Socket *socket)
{

        PipePair *pipe_pair = new WANProxyCodecPipePair(interface_codec_,
                                                        remote_codec_);

        switch (peer_proto_) {
        case ConfigProtoTCP:
                new ProxyConnector(name_, pipe_pair, socket,
                                        remote_family_, remote_name_);
                break;
        case ConfigProtoTCPPool: {
                new ProxyConnector(name_, pipe_pair, socket, socket_pool);
                break;
        }
        default:
                HALT("ProxyListenner") << "proto not supported";
        }

}

