/*
 * Copyright (c) 2009 Juli Mallett. All rights reserved.
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

#include "wanproxy_config_type_codec.h"

static struct WANProxyConfigTypeCodec::Mapping wanproxy_config_type_codec_map[] = {
	{ "XCodec",	WANProxyConfigCodecXCodec },
	{ "None",	WANProxyConfigCodecNone },
	{ NULL,		WANProxyConfigCodecNone }
};

WANProxyConfigTypeCodec
	wanproxy_config_type_codec("codec", wanproxy_config_type_codec_map);

static struct WANProxyConfigTypeCache::Mapping wanproxy_config_type_cache_map[] = {
	{ "Memory",	WANProxyConfigCacheMemory },
	{ "COSS",	WANProxyConfigCacheCOSS },
	{ NULL,		WANProxyConfigCacheMemory }
};

WANProxyConfigTypeCache
	wanproxy_config_type_cache("cache", wanproxy_config_type_cache_map);
