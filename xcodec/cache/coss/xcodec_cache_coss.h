
#ifndef	XCODEC_XCODEC_CACHE_COSS_H
#define	XCODEC_XCODEC_CACHE_COSS_H

/*
 *
 * XCodec COSS Cache
 * 
 * COSS = Cyclic Object Storage System
 *
 * Idea taken from Squid COSS.
 * 
 * Diego Woitasen <diegows@xtech.com.ar>
 * XTECH
 *
 */

#include <string>
#include <map>
#include <fstream>
#include <iostream>

#include <common/buffer.h>
#include <xcodec/xcodec.h>
#include <xcodec/xcodec_cache.h>

using namespace std;

/*
 * - In COSS, we have one file per cache (UUID). The file is divided in 
 * stripes.
 *
 * - Each stripe is composed by:
 * metadata + hash array + segment size array + segment array.
 *
 * - The arrays elements are the same order:
 * hash1, hash2, ..., hashN - size1, size2, ..., sizeN - seg1, seg2, ..., segN.
 *
 * - The segments are indexed in memory. This index is loaded when the cache is
 * openned, reading the hash array of each stripe. Takes a few millisecons in 
 * a 10 GB cache.
 *
 * - We have one active stripe in memory at a time. New segments are written
 * in the current stripe in order of appearance.
 *
 * - When a cached segment is requested and it's out of the active stripe, 
 * is copied to it.
 *
 * - When the current stripe is full, we move to the next one.
 *
 * - When we reach the EOF, first stripe is zeroed and becomes active.
 *
 */

/*
 * This values should be page aligned.
 */
#define METADATA_BYTES 4096
#define ARRAY_SIZE 2048

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

/*
 * TODO:
 * - Implement ref count for some of the data structures used here.
 */

struct COSSIndexEntry {
        uint64_t stripe_number;
        uint32_t pos;
	uint32_t length;
        COSSIndexEntry(uint64_t stripe_number_, uint32_t pos_, uint32_t length_) :
                stripe_number(stripe_number_),
                pos(pos_),
		length(length_) { }
};

class COSSIndex {
	typedef __gnu_cxx::hash_map<Hash64, const COSSIndexEntry *> index_t;
	index_t index;

	/*
         * multimap of (stripe_number, hash) pairs. Used keep track of the
         * segments in each stripe to delete them from the index when
         * the stripe is going to be overwritten.
	 */
	multimap<uint64_t, Hash64> stripe_segments;
	typedef multimap<uint64_t, Hash64>::const_iterator stripe_segments_it;

	LogHandle log_;

public:
	COSSIndex() : log_("/xcodec/cache/coss/") {}

	~COSSIndex() {
		/*
		TODO: 
		there are other things to clear here, for example stripe_segment.
		*/
		index_t::iterator it;
		for (it = index.begin(); it != index.end(); ++it) {
			delete it->second;
		}
	}


	void insert(const uint64_t &hash, const COSSIndexEntry *entry)
	{
		index_t::iterator it = index.find(hash);
		if (it != index.end())
			delete it->second;
		index[hash] = entry;
		stripe_segments.insert(make_pair(entry->stripe_number, hash));
	}

	const COSSIndexEntry *get(const uint64_t &hash)
	{
		index_t::iterator it = index.find(hash);
		const COSSIndexEntry *entry;
		if (it == index.end())
			return NULL;
		entry = it->second;
		return entry;
	}

	size_t size()
	{
		return index.size();
	}

	void delete_stripe(uint64_t stripe_number)
	{
		pair<stripe_segments_it, stripe_segments_it> range;
		const COSSIndexEntry *index_entry;

		range = stripe_segments.equal_range(stripe_number);

		for(stripe_segments_it i = range.first; i != range.second; i++)
		{
			Hash64 hash = i->second;

			// Check if the segment is the index.
			index_t::iterator it = index.find(hash);
			if (it == index.end())
				continue;

			index_entry = it->second;
			/* segment was relocated, ignore */
			if (index_entry->stripe_number != stripe_number)
				continue;

			delete index[hash];
			index.erase(hash);
		}
		stripe_segments.erase(stripe_number);
	}

};

struct COSSOnDiskSegment {
	uint8_t bytes[BUFFER_SEGMENT_SIZE];
	string hexdump() {
		string dump;
		char buf[8];
		int i;
		for (i = 0; i < 70; i++) {
			snprintf(buf, 8, "%02x", bytes[i]);
			dump += buf;
		}
		return dump;
	}
};

struct COSSMetadata {
	/*
         * Each stripe has a auto incremental serial number to identify the
         * last active stripe when the file is openned.
	 * Right now, only one member with some space reserved for future use.
         */
	uint64_t serial; 
	char _padding_[METADATA_BYTES - sizeof(uint64_t)];
};

struct COSSStripeHeader {
	COSSMetadata metadata;
	uint64_t hash_array[ARRAY_SIZE];
	uint32_t size_array[ARRAY_SIZE];	
};

struct COSSStripe_ {
	COSSStripeHeader header;
	COSSOnDiskSegment segment_array[ARRAY_SIZE];
};

struct COSSStats {
	uint64_t lookups;
	uint64_t hits;
	uint64_t misses;
	
	COSSStats() : lookups(0), hits(0), misses(0) {}
};

class COSSStripe {
	COSSStripe_ stripe;

	uint32_t stripe_size;
	uint64_t number;
	uint32_t current_pos;
	uint32_t last_pos;

	LogHandle log_;

	COSSIndexEntry *append_(const uint64_t &hash, const uint32_t length_)
	{

		COSSIndexEntry *new_pos;

		new_pos = new COSSIndexEntry(number, current_pos, length_);

		stripe.header.hash_array[current_pos] = hash;
		stripe.header.size_array[current_pos] = length_;

		current_pos++;

		return new_pos;
	}

public:
	COSSStripe() : log_("xcodec/cache/coss/")
	{

		last_pos = ARRAY_SIZE - 1;
		stripe_size = sizeof(COSSStripe_);
		reset(0, 0);

	}

	~COSSStripe()
	{
		DEBUG(log_) << "~COSSStripe() current_pos = " 
				<< current_pos << endl;
	}

	COSSIndexEntry *append(const uint64_t &hash, BufferSegment *seg)
	{

		uint8_t *segment;

		ASSERT(log_, !full());

		segment = (uint8_t *)stripe.segment_array[current_pos].bytes;
		seg->copyout(segment, 0, seg->length());

		return append_(hash, seg->length());
		
	}

	COSSIndexEntry *append(const uint64_t &hash, COSSOnDiskSegment *seg, 
					uint32_t length_)
	{


		ASSERT(log_, !full());

		stripe.segment_array[current_pos] = *seg;

		return append_(hash, length_);
		
	}

	uint8_t *data() 
	{
		return (uint8_t *)&stripe;
	}

	size_t length() const
	{
		return stripe_size;
	}

	bool full() const
	{
		return (current_pos > last_pos);
	}
	
	uint64_t get_number() const
	{
		return number;
	}

	uint64_t get_hash(uint32_t pos) const
	{

		return stripe.header.hash_array[pos];
	}

	COSSOnDiskSegment *get_segment(uint32_t pos) const
	{
		return (COSSOnDiskSegment *)stripe.segment_array[pos].bytes;
	}


	void reset(uint64_t serial, uint64_t number_) 
	{
		memset((uint8_t *)&stripe, 0, stripe_size);
		stripe.header.metadata.serial = serial;
		number = number_;
		current_pos = 0;
	}

	void set_pos(uint32_t pos)
	{
		DEBUG(log_) << "pos=" << pos << endl;
		current_pos = pos;
	}
	
	void dump() const
	{
		int i;
		DEBUG(log_) << " - " << ARRAY_SIZE;
		for (i = 0; i < ARRAY_SIZE; i++) {
			DEBUG(log_) << "Hash: " << stripe.header.hash_array[i];
			if (stripe.header.hash_array[i])
				break;
		}
	}
};


class XCodecCacheCOSS : public XCodecCache {
	std::string cache_dir;
	std::string coss_file;
	//TODO: remove this after we improve the configuration system.
	uint64_t cache_size_;
	uint64_t local_size_;
	uint64_t remote_size_;

	fstream coss_fd;

	uint64_t coss_size;
	uint64_t last_stripe_n;
	uint64_t stripe_number;
	uint64_t serial; 

	COSSStripe *active;

	COSSIndex index;

	LogHandle log_;

	COSSStats stats;

	COSSOnDiskSegment *get_segment(const COSSIndexEntry *index_entry);
	void new_active(bool write =1);
	void read_file();
	uint64_t file_size() {
		uint64_t pos, ret;
		
		pos = coss_fd.tellg();
		coss_fd.seekg(0, ios::end);
		ret = coss_fd.tellg();
		coss_fd.seekg(pos);
		return ret;
		
	}
public:
	XCodecCacheCOSS(const UUID& uuid, const std::string &cache_dir_, 
		const uint64_t cache_size_,
		const uint64_t local_size_,
		const uint64_t remote_size_);

	~XCodecCacheCOSS();

	void enter(const uint64_t& hash, BufferSegment *seg);

	XCodecCache *new_uuid(const UUID &uuid) const;

	bool out_of_band(void) const
	{
		/*
		 * Memory caches are not exchanged out-of-band; references
		 * must be extracted in-stream.
		 */
		return (false);
	}

	BufferSegment *lookup(const uint64_t& hash);

};

#endif /* !XCODEC_XCODEC_CACHE_COSS_H */

