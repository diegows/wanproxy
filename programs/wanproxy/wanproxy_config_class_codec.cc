/*
 * Copyright (c) 2009-2013 Juli Mallett. All rights reserved.
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

#include <common/buffer.h>

#include <config/config_class.h>
#include <config/config_object.h>

#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>
#include <xcodec/cache/coss/xcodec_cache_coss.h>

#include <event/event_callback.h>
#include <event/event_system.h>
#include <io/socket/socket.h>

#include "wanproxy_config_class_codec.h"

#include <fstream>

WANProxyConfigClassCodec wanproxy_config_class_codec;

XCodecCache *
WANProxyConfigClassCodec::Instance::new_codec_cache(UUID &uuid)
{
	XCodecCache *cache;

	switch(cache_type){
	case WANProxyConfigCacheMemory:
		cache = new XCodecMemoryCache(uuid);
		return cache;
	case WANProxyConfigCacheCOSS: {
		INFO("/wanproxy/config/cache/path") << cache_path;
		INFO("/wanproxy/config/cache/local_size") << local_size;
		INFO("/wanproxy/config/cache/remote_size") << remote_size;

		cache = new XCodecCacheCOSS(uuid, cache_path, local_size, local_size,
                                                remote_size);
		return cache;

		}
	default:
		ASSERT("xcodec/cache/new", false);
	}

	// XXX: to keep g++ happy, but we must never reach this point.
	return cache;

}

bool
WANProxyConfigClassCodec::Instance::activate(const ConfigObject *co)
{
	codec_.name_ = co->name_;

	switch (codec_type_) {
	case WANProxyConfigCodecXCodec: {
		/*
		 * Fetch UUID from permanent storage if there is any.
		 */
		UUID uuid;
		std::string uuid_path = cache_path + "/UUID";
		std::fstream uuid_file;

		if(cache_path.empty())
			uuid.generate();
		else{   
			if(!uuid.load_from_file(uuid_path)){
				uuid.generate();
				uuid.save_to_file(uuid_path);
			}
		}

		INFO("/wanproxy/config/cache/uuid") << uuid.string_;

		XCodecCache *cache = XCodecCache::lookup(uuid);
		if (cache == NULL) {
			cache = new_codec_cache(uuid);
			XCodecCache::enter(uuid, cache);
		}
		//XXX: this is broken, because we could have different
		//definitions, there is no main codec. We should find
		//a clean way to link encoder/decoder caches.
		XCodecCache::set_local(cache);
		XCodec *xcodec = new XCodec(cache);

		codec_.codec_ = xcodec;
		break;
	}
	case WANProxyConfigCodecNone:
		codec_.codec_ = NULL;
		break;
	default:
		ERROR("/wanproxy/config/codec") << "Invalid codec type.";
		return (false);
	}

	switch (compressor_) {
	case WANProxyConfigCompressorZlib:
		if (compressor_level_ < 0 || compressor_level_ > 9) {
			ERROR("/wanproxy/config/codec") << "Compressor level must be in range 0..9 (inclusive.)";
			return (false);
		}

		codec_.compressor_ = true;
		codec_.compressor_level_ = compressor_level_;
		break;
	case WANProxyConfigCompressorNone:
		if (compressor_level_ != -1) {
			ERROR("/wanproxy/config/codec") << "Compressor level set but no compressor.";
			return (false);
		}

		codec_.compressor_ = false;
		codec_.compressor_level_ = 0;
		break;
	default:
		ERROR("/wanproxy/config/codec") << "Invalid compressor type.";
		return (false);
	}

        //XXX: other thing to move to a better place. diegows
        //SimpleCallback *scb = callback(this, &WANProxyConfigClassCodec::close_caches);
        //stop_action_ =
        //EventSystem::instance()->register_interest(EventInterestStop, scb);


	return (true);
}

