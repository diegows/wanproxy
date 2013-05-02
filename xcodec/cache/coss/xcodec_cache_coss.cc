/*
 *
 * XCodec COSS Cache
 *
 * COSS = Cyclic Object storage system
 *
 * Idea taken from Squid COSS cache.
 * 
 * Diego Woitasen <diegows@xtech.com.ar>
 * XTECH
 *
 */

#include <xcodec/cache/coss/xcodec_cache_coss.h>

#include <boost/filesystem.hpp>
#include <iostream>

#include <string.h>

using namespace boost::filesystem;

XCodecCacheCOSS::XCodecCacheCOSS(const UUID &uuid, const std::string &cache_dir_,
		const uint64_t cache_size,
		const uint64_t local_size,
		const uint64_t remote_size)
	: XCodecCache(uuid) , 
                cache_size_(cache_size), 
                local_size_(local_size),
                remote_size_(remote_size),
                log_("xcodec/cache/coss")
{ 

	cache_dir = cache_dir_;
	coss_file = cache_dir_ + "/" + uuid.string_ + ".coss";

	if (!exists(coss_file))
		ofstream tmp_fd(coss_file.c_str());

	coss_fd.exceptions(fstream::failbit | fstream::badbit);
	coss_fd.open(coss_file.c_str(), fstream::in | fstream::out | fstream::binary);
	ASSERT(log_, coss_fd.good());

	active = new COSSStripe();
	serial = 0;
	stripe_number = 0;
	
	coss_size = ROUND_UP(cache_size_ * 1048576, active->length());
	last_stripe_n = coss_size / active->length() - 1;

	if (file_size() > 0)
		read_file();

	INFO(log_) << "File: " << coss_file;
	INFO(log_) << "Size: " << coss_size;
	INFO(log_) << "Stripe size: " << active->length();
	INFO(log_) << "Stripe header size: " << sizeof(COSSStripeHeader);

	DEBUG(log_) << "serial=" << serial;
	DEBUG(log_) << "stripe_number=" << stripe_number;

}

XCodecCacheCOSS::~XCodecCacheCOSS()
{

	coss_fd.write((char *)active->data(), active->length());
        coss_fd.close();

	delete active;

	INFO(log_) << "Closing coss file: " << coss_file;
	INFO(log_) << "Stats: ";
	INFO(log_) << "\tLookups=" << stats.lookups;
	INFO(log_) << "\tHits=" << stats.hits;
	INFO(log_) << "\tMisses=" << stats.misses;
        if (stats.lookups > 0)
                INFO(log_) << "\tHit ratio=" << (stats.hits * 100) /stats.lookups;

	DEBUG(log_) << "serial=" << serial;
	DEBUG(log_) << "stripe_number=" << stripe_number;
	DEBUG(log_) << "index size=" << index.size();

}

void
XCodecCacheCOSS::enter(const uint64_t &hash, BufferSegment *seg)
{
	ASSERT(log_, seg->length() == XCODEC_SEGMENT_LENGTH);

	if (active->full())
		new_active();

	index.insert(hash, active->append(hash, seg));

}

BufferSegment *
XCodecCacheCOSS::lookup(const uint64_t& hash)
{

	BufferSegment *seg;
	COSSOnDiskSegment *on_disk_seg;
	COSSIndexEntry *index_entry;

        stats.lookups++;

	index_entry = const_cast<COSSIndexEntry *>(index.get(hash));
	if (!index_entry) {
                stats.misses++;
		return NULL;
        }
        else {
                stats.hits++;
        }
	
	if (index_entry->stripe_number == active->get_number()){
		ASSERT(log_, active->get_hash(index_entry->pos) == hash);
		on_disk_seg = active->get_segment(index_entry->pos);
	}
	else
	{
                uint32_t len = index_entry->length;
		on_disk_seg = get_segment(index_entry);
		if (active->full())
			new_active();
                index_entry = active->append(hash, on_disk_seg, len);
                // XXX fix
                delete on_disk_seg;
		on_disk_seg = active->get_segment(index_entry->pos);
		index.insert(hash, index_entry);
	}

	seg = BufferSegment::create((uint8_t *)on_disk_seg->bytes,
					index_entry->length);

	return seg;

}

XCodecCache *
XCodecCacheCOSS::new_uuid(const UUID &uuid) const
{

	return new XCodecCacheCOSS(uuid, this->cache_dir, remote_size_,
                                        local_size_, remote_size_);
}

COSSOnDiskSegment *
XCodecCacheCOSS::get_segment(const COSSIndexEntry *index_entry)
{

	COSSOnDiskSegment *on_disk_seg = new COSSOnDiskSegment();
	streampos pos;
	streampos segment_pos;

	pos = coss_fd.tellg();

	segment_pos = index_entry->stripe_number * active->length() +
                        sizeof(COSSStripeHeader) +
                        (sizeof(COSSOnDiskSegment) * index_entry->pos);

	coss_fd.seekg(segment_pos);
	coss_fd.read((char *)on_disk_seg->bytes, index_entry->length);
	coss_fd.seekg(pos);

	return on_disk_seg;

}

void
XCodecCacheCOSS::new_active(bool write)
{

	if (write)
		coss_fd.write((char *)active->data(), active->length());
	if (stripe_number == last_stripe_n){
		coss_fd.seekg(0);
		stripe_number = 0;
	}
	else
		stripe_number++;
	serial++;
	active->reset(serial, stripe_number);
        index.delete_stripe(stripe_number);

}

void
XCodecCacheCOSS::read_file()
{

	COSSStripeHeader header;
	COSSStripeHeader max_header;
        COSSIndexEntry *index_entry;
	streampos stripe_pos;
	uint64_t max_stripe_number = 0;
	int i;
        uint64_t hash;
        uint32_t size;

	coss_fd.seekg(0);

	try {
		while (!coss_fd.eof()) {
			coss_fd.read((char *)&header, sizeof(header));
			coss_fd.seekg(sizeof(COSSOnDiskSegment) * ARRAY_SIZE,
					ios::cur);

			if (header.metadata.serial >= serial) {
				serial = header.metadata.serial;
				max_stripe_number = stripe_number;
				max_header = header;
			}

                        for (i = 0; i < ARRAY_SIZE; i++) {
                                hash = header.hash_array[i];
                                if (!hash)
                                        break;
                                size = header.size_array[i];
                                index_entry = new COSSIndexEntry(stripe_number,
                                                        i,
                                                        size);
                                index.insert(hash, index_entry);
                        }

			stripe_number++;
		}
	}
	catch (ifstream::failure e) {
		ASSERT(log_, !coss_fd.bad());
		coss_fd.clear();	
	}

	if (serial == 0)
		stripe_number = 0;
	else
		stripe_number = max_stripe_number;

	active->reset(serial, stripe_number);

	for (i = 0; i < ARRAY_SIZE; i++) {
		if (max_header.hash_array[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE && max_header.hash_array[i] == 0) {
		new_active(0);
		stripe_pos = stripe_number * active->length();
		coss_fd.seekg(stripe_pos);
	}
	else {
		stripe_pos = stripe_number * active->length();
		coss_fd.seekg(stripe_pos);
		coss_fd.read((char *)active->data(), active->length());
		coss_fd.seekg(stripe_pos);
		active->set_pos(i);
	}

}

