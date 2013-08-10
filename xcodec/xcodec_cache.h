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

#ifndef	XCODEC_XCODEC_CACHE_H
#define	XCODEC_XCODEC_CACHE_H

#include <ext/hash_map>
#include <map>

#include <common/uuid/uuid.h>
#include <xcodec/xcodec.h>

/*
 * XXX
 * GCC supports hash<unsigned long> but not hash<unsigned long long>.  On some
 * of our platforms, the former is 64-bits, on some the latter.  As a result,
 * we need this wrapper structure to throw our hashes into so that GCC's hash
 * function can be reliably defined to use them.
 */
struct Hash64 {
	uint64_t hash_;

	Hash64(const uint64_t& hash)
	: hash_(hash)
	{ }

	bool operator== (const Hash64& hash) const
	{
		return (hash_ == hash.hash_);
	}

	bool operator< (const Hash64& hash) const
	{
		return (hash_ < hash.hash_);
	}
};

namespace __gnu_cxx {
	template<>
	struct hash<Hash64> {
		size_t operator() (const Hash64& x) const
		{
			return (x.hash_);
		}
	};
}

class XCodecCache {
protected:
	UUID uuid_;

	XCodecCache(const UUID& uuid)
	: uuid_(uuid)
	{ }

public:
	virtual ~XCodecCache()
	{ }

	virtual void enter(const uint64_t&, BufferSegment *) = 0;
	virtual BufferSegment *lookup(const uint64_t&) = 0;
	virtual bool out_of_band(void) const = 0;

	/* 
	 * Return the a new object of the same type with using another UUID.
	 * Useful to open a new cache in the decoder, using the same type of cache
	 * of the encoder. We should improve this anyway!.
	 */
	virtual XCodecCache *new_uuid(const UUID &uuid) const = 0;

	//XXX: these two function should be removed. See XXX comment in
	//wanproxy_config_class_codec.cc. diegows
	static void set_local(XCodecCache *cache)
	{
		INFO("XCodecCache set_local") << cache;
		local_cache = cache;
	}

	static XCodecCache *get_local()
	{
		INFO("XCodecCache get_local") << local_cache;
		return local_cache;
	}

	bool uuid_encode(Buffer *buf) const
	{
		return (uuid_.encode(buf));
	}

	static void enter(const UUID& uuid, XCodecCache *cache)
	{
		ASSERT("/xcodec/cache", cache_map.find(uuid) == cache_map.end());
		cache_map[uuid] = cache;
	}

	static XCodecCache *lookup(const UUID& uuid)
	{
		std::map<UUID, XCodecCache *>::const_iterator it;

		it = cache_map.find(uuid);
		if (it == cache_map.end())
			return (NULL);

		return (it->second);
	}

	static void close_caches(void)
	{
		std::map<UUID, XCodecCache *>::iterator it;
		
		INFO("/xcodec/cache") << "Closing caches.";

		for (it = cache_map.begin(); it != cache_map.end(); it++) {
			delete it->second;
		}

		cache_map.clear();
	}

private:
	static std::map<UUID, XCodecCache *> cache_map;
	static XCodecCache *local_cache;
};

class XCodecMemoryCache : public XCodecCache {
	typedef __gnu_cxx::hash_map<Hash64, BufferSegment *> segment_hash_map_t;

	LogHandle log_;
	segment_hash_map_t segment_hash_map_;
public:
	XCodecMemoryCache(const UUID& uuid)
	: XCodecCache(uuid),
	  log_("/xcodec/cache/memory"),
	  segment_hash_map_()
	{ }

	~XCodecMemoryCache()
	{
		segment_hash_map_t::const_iterator it;
		for (it = segment_hash_map_.begin();
		     it != segment_hash_map_.end(); ++it)
			it->second->unref();
		segment_hash_map_.clear();
	}

	void enter(const uint64_t& hash, BufferSegment *seg)
	{
		ASSERT(log_, seg->length() == XCODEC_SEGMENT_LENGTH);
		ASSERT(log_, segment_hash_map_.find(hash) == segment_hash_map_.end());
		seg->ref();
		segment_hash_map_[hash] = seg;
	}

	bool out_of_band(void) const
	{
		/*
		 * Memory caches are not exchanged out-of-band; references
		 * must be extracted in-stream.
		 */
		return (false);
	}

	BufferSegment *lookup(const uint64_t& hash)
	{
		segment_hash_map_t::const_iterator it;
		it = segment_hash_map_.find(hash);
		if (it == segment_hash_map_.end())
			return (NULL);

		BufferSegment *seg;

		seg = it->second;
		seg->ref();
		return (seg);
	}

	XCodecCache *new_uuid(const UUID &uuid) const
	{

		return new XCodecMemoryCache(uuid);

	}
};

#endif /* !XCODEC_XCODEC_CACHE_H */
