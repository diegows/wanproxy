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

#ifndef	PROGRAMS_WANPROXY_PROXY_LISTENER_H
#define	PROGRAMS_WANPROXY_PROXY_LISTENER_H

#include <io/socket/simple_server.h>
#include <io/net/tcp_pool_client.h>
#include <config/config_type_proto.h>

class Socket;
class TCPServer;
struct WANProxyCodec;

class ProxyListener : public SimpleServer<TCPServer> {
	std::string name_;
	WANProxyCodec *interface_codec_;
	WANProxyCodec *remote_codec_;
	SocketAddressFamily remote_family_;
	TCPClientSocketPool *socket_pool;
	std::string remote_name_;

	ConfigProto interface_proto_;
	ConfigProto peer_proto_;

	Action *read_notify_action_;

	void accept_callback(Event, SocketPoolAction *);
	void start_connector(Socket *);

public:
	ProxyListener(const std::string&, WANProxyCodec *, WANProxyCodec *, SocketAddressFamily,
		      const std::string&, SocketAddressFamily,
		      const std::string&, ConfigProto, ConfigProto);
	~ProxyListener();

private:
	void client_connected(Socket *);
};

#endif /* !PROGRAMS_WANPROXY_PROXY_LISTENER_H */
