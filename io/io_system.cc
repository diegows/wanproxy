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

#include <sys/errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>

#include <common/limits.h>

#include <event/event_callback.h>
#include <event/event_system.h>

#include <io/io_system.h>

#define	IO_READ_BUFFER_SIZE	65536

IOSystem::Handle::Handle(int fd, Channel *owner)
: log_("/io/system/handle"),
  fd_(fd),
  owner_(owner),
  close_callback_(NULL),
  close_action_(NULL),
  read_offset_(-1),
  read_amount_(0),
  read_buffer_(),
  read_callback_(NULL),
  read_action_(NULL),
  write_offset_(-1),
  write_buffer_(),
  write_callback_(NULL),
  write_action_(NULL)
{ }

IOSystem::Handle::~Handle()
{
	ASSERT(log_, fd_ == -1);

	ASSERT(log_, close_action_ == NULL);
	ASSERT(log_, close_callback_ == NULL);

	ASSERT(log_, read_action_ == NULL);
	ASSERT(log_, read_callback_ == NULL);

	ASSERT(log_, write_action_ == NULL);
	ASSERT(log_, write_callback_ == NULL);
}

void
IOSystem::Handle::close_callback(void)
{
	close_action_->cancel();
	close_action_ = NULL;

	ASSERT(log_, fd_ != -1);
	int rv = ::close(fd_);
	if (rv == -1) {
		if (errno == EAGAIN) {
			/*
			 * XXX
			 * Is there something we should be polling on?
			 */
			close_action_ = close_schedule();
			return;
		}

		/*
		 * We display the error here but do not pass it to the
		 * upper layers because it is almost impossible to get
		 * handling of close failing correct.
		 *
		 * For most errors, close fails because it already was
		 * closed by a peer or something like that, so it's as
		 * good as close succeeding.  Only in the case where we
		 * get an EAGAIN, in which case we loop internally,
		 * should anything more be done.
		 */
		ERROR(log_) << "Close returned error: " << strerror(errno);
	}
	fd_ = -1;
	Action *a = close_callback_->schedule();
	close_action_ = a;
	close_callback_ = NULL;
}

void
IOSystem::Handle::close_cancel(void)
{
	ASSERT(log_, close_action_ != NULL);
	close_action_->cancel();
	close_action_ = NULL;

	if (close_callback_ != NULL) {
		delete close_callback_;
		close_callback_ = NULL;
	}
}

Action *
IOSystem::Handle::close_schedule(void)
{
	ASSERT(log_, close_action_ == NULL);
	SimpleCallback *cb = callback(this, &IOSystem::Handle::close_callback);
	Action *a = cb->schedule();
	return (a);
}

void
IOSystem::Handle::read_callback(Event e)
{
	read_action_->cancel();
	read_action_ = NULL;

	switch (e.type_) {
	case Event::EOS:
	case Event::Done:
		break;
	case Event::Error: {
		DEBUG(log_) << "Poll returned error: " << e;
		read_callback_->param(e);
		Action *a = read_callback_->schedule();
		read_action_ = a;
		read_callback_ = NULL;
		return;
	}
	default:
		HALT(log_) << "Unexpected event: " << e;
	}

	read_action_ = read_do();
	if (read_action_ == NULL)
		read_action_ = read_schedule();
	ASSERT(log_, read_action_ != NULL);
}

void
IOSystem::Handle::read_cancel(void)
{
	ASSERT(log_, read_action_ != NULL);
	read_action_->cancel();
	read_action_ = NULL;

	if (read_callback_ != NULL) {
		delete read_callback_;
		read_callback_ = NULL;
	}
}

Action *
IOSystem::Handle::read_do(void)
{
	ASSERT(log_, read_action_ == NULL);

	if (!read_buffer_.empty() && read_buffer_.length() >= read_amount_) {
		if (read_amount_ == 0)
			read_amount_ = read_buffer_.length();
		read_callback_->param(Event(Event::Done, Buffer(read_buffer_, read_amount_)));
		Action *a = read_callback_->schedule();
		read_callback_ = NULL;
		read_buffer_.skip(read_amount_);
		read_amount_ = 0;
		return (a);
	}

	/*
	 * A bit of discussion is warranted on this:
	 *
	 * In tack, IOV_MAX BufferSegments are allocated and read in to
	 * with readv(2), and then the lengths are adjusted and the ones
	 * that are empty are freed.  It's also possible to set the
	 * expected lengths first (and only allocate
	 * 	roundup(rlen, BUFFER_SEGMENT_SIZE) / BUFFER_SEGMENT_SIZE
	 * BufferSegments, though really IOV_MAX (or some chosen number)
	 * seems a bit better since most of our reads right now are
	 * read_amount_==0) and put them into a Buffer and trim the
	 * leftovers, which is a bit nicer.
	 *
	 * Since our read_amount_ is usually 0, though, we're kind of at
	 * the mercy of chance (well, not really) as to how much data we
	 * will read, which means a sizable amount of thrashing of memory;
	 * allocating and freeing BufferSegments.
	 *
	 * By comparison, stack space is cheap in userland and allocating
	 * 64K of it here is pretty painless.  Reading to it is fast and
	 * then copying only what we need into BufferSegments isn't very
	 * costly.  Indeed, since readv can't sparsely-populate each data
	 * pointer, it has to do some data shuffling, already.
	 *
	 * Benchmarking used to show that readv was actually markedly
	 * slower here, primarily because of the need to new and delete
	 * lots of BufferSegments.  Now that there is a BufferSegment
	 * cache, that cost is significantly lowered.  It is probably a
	 * good idea to reevaluate it now, especially if we can stomach
	 * also keeping a small cache of BufferSegments just for this
	 * IOSystem Handle.
	 */
	uint8_t data[IO_READ_BUFFER_SIZE];
	ssize_t len;
	if (read_offset_ == -1) {
		len = ::read(fd_, data, sizeof data);
	} else {
		/*
		 * For offset reads, we do not read extra data since we do
		 * not know whether the next read will be to the subsequent
		 * location.
		 *
		 * This makes even more sense since we don't allow 0-length
		 * offset reads.
		 */
		size_t size = std::min(sizeof data, read_amount_);
		len = ::pread(fd_, data, size, read_offset_);
		if (len > 0)
			read_offset_ += len;
	}
	if (len == -1) {
		switch (errno) {
		case EAGAIN:
			return (NULL);
		default:
			read_callback_->param(Event(Event::Error, errno, read_buffer_));
			Action *a = read_callback_->schedule();
			read_callback_ = NULL;
			read_buffer_.clear();
			read_amount_ = 0;
			return (a);
		}
		NOTREACHED(log_);
	}

	/*
	 * XXX
	 * If we get a short read from readv and detected EOS from
	 * EventPoll is that good enough, instead?  We can keep reading
	 * until we get a 0, sure, but if things other than network
	 * conditions influence whether reads would block (and whether
	 * non-blocking reads return), there could be more data waiting,
	 * and so we shouldn't just use a short read as an indicator?
	 */
	if (len == 0) {
		read_callback_->param(Event(Event::EOS, read_buffer_));
		Action *a = read_callback_->schedule();
		read_callback_ = NULL;
		read_buffer_.clear();
		read_amount_ = 0;
		return (a);
	}

	read_buffer_.append(data, len);

	if (!read_buffer_.empty() &&
	    read_buffer_.length() >= read_amount_) {
		if (read_amount_ == 0)
			read_amount_ = read_buffer_.length();
		read_callback_->param(Event(Event::Done, Buffer(read_buffer_, read_amount_)));
		Action *a = read_callback_->schedule();
		read_callback_ = NULL;
		read_buffer_.skip(read_amount_);
		read_amount_ = 0;
		return (a);
	}

	/*
	 * We may do another read without polling, but yield to other
	 * callbacks.
	 */
	if (len == sizeof data) {
		/* TODO */
		DEBUG(log_) << "Could read without polling.";
	}

	return (NULL);
}

Action *
IOSystem::Handle::read_schedule(void)
{
	ASSERT(log_, read_action_ == NULL);

	EventCallback *cb = callback(this, &IOSystem::Handle::read_callback);
	Action *a = EventSystem::instance()->poll(EventPoll::Readable, fd_, cb);
	return (a);
}

Action *
IOSystem::Handle::read_schedule(EventCallback *cb)
{
	ASSERT(log_, read_action_ == NULL);

	Action *a = EventSystem::instance()->poll(EventPoll::Readable, fd_, cb);
	return (a);
}

void
IOSystem::Handle::write_callback(Event e)
{
	write_action_->cancel();
	write_action_ = NULL;

	switch (e.type_) {
	case Event::Done:
		break;
	case Event::Error: {
		DEBUG(log_) << "Poll returned error: " << e;
		write_callback_->param(e);
		Action *a = write_callback_->schedule();
		write_action_ = a;
		write_callback_ = NULL;
		return;
	}
	default:
		HALT(log_) << "Unexpected event: " << e;
	}

	write_action_ = write_do();
	if (write_action_ == NULL)
		write_action_ = write_schedule();
	ASSERT(log_, write_action_ != NULL);
}

void
IOSystem::Handle::write_cancel(void)
{
	ASSERT(log_, write_action_ != NULL);
	write_action_->cancel();
	write_action_ = NULL;

	if (write_callback_ != NULL) {
		delete write_callback_;
		write_callback_ = NULL;
	}
}

Action *
IOSystem::Handle::write_do(void)
{
	/*
	 * XXX
	 *
	 * This doesn't handle UDP nicely.  Right?
	 *
	 * If a UDP packet is > IOV_MAX segments, this will break it.
	 * Need something like mbuf(9)'s m_collapse(), where we can demand
	 * that the Buffer fit into IOV_MAX segments, rather than saying
	 * that we want the first IOV_MAX segments.  Easy enough to combine
	 * the unshared BufferSegments?
	 */
	struct iovec iov[IOV_MAX];
	size_t iovcnt = write_buffer_.fill_iovec(iov, IOV_MAX);
	ASSERT(log_, iovcnt != 0);

	ssize_t len;
	if (write_offset_ == -1) {
		len = ::writev(fd_, iov, iovcnt);
	} else {
#if defined(__FreeBSD__)
		len = ::pwritev(fd_, iov, iovcnt, write_offset_);
		if (len > 0)
			write_offset_ += len;
#else
		/*
		 * XXX
		 * Thread unsafe.
		 */
		off_t off = lseek(fd_, write_offset_, SEEK_SET);
		if (off == -1) {
			len = -1;
		} else {
			len = ::writev(fd_, iov, iovcnt);
			if (len > 0)
				write_offset_ += len;
		}

		/*
		 * XXX
		 * Slow!
		 */
#if 0
		unsigned i;

		if (iovcnt == 0) {
			len = -1;
			errno = EINVAL;
		}

		for (i = 0; i < iovcnt; i++) {
			struct iovec *iovp = &iov[i];

			ASSERT(log_, iovp->iov_len != 0);

			len = ::pwrite(fd_, iovp->iov_base, iovp->iov_len,
				       write_offset_);
			if (len <= 0)
				break;

			write_offset_ += len;

			/*
			 * Partial write.
			 */
			if ((size_t)len != iovp->iov_len)
				break;
		}
#endif
#endif
	}
	if (len == -1) {
		switch (errno) {
		case EAGAIN:
			return (NULL);
		default:
			write_callback_->param(Event(Event::Error, errno));
			Action *a = write_callback_->schedule();
			write_callback_ = NULL;
			return (a);
		}
		NOTREACHED(log_);
	}

	write_buffer_.skip(len);

	if (write_buffer_.empty()) {
		write_callback_->param(Event::Done);
		Action *a = write_callback_->schedule();
		write_callback_ = NULL;
		return (a);
	}
	return (NULL);
}

Action *
IOSystem::Handle::write_schedule(void)
{
	ASSERT(log_, write_action_ == NULL);
	EventCallback *cb = callback(this, &IOSystem::Handle::write_callback);
	Action *a = EventSystem::instance()->poll(EventPoll::Writable, fd_, cb);
	return (a);
}

IOSystem::IOSystem(void)
: log_("/io/system"),
  handle_map_()
{
	/*
	 * Prepare system to handle IO.
	 */
	INFO(log_) << "Starting IO system.";

	/*
	 * Disable SIGPIPE.
	 *
	 * Because errors are returned asynchronously and may occur at any
	 * time, there may be a pending write to a file descriptor which
	 * has previously thrown an error.  There are changes that could
	 * be made to the scheduler to work around this, but they are not
	 * desirable.
	 */
	if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		HALT(log_) << "Could not disable SIGPIPE.";

	/*
	 * Up the file descriptor limit.
	 *
	 * Probably this should be configurable, but there's no harm on
	 * modern systems and for the performance-critical applications
	 * using the IO system, more file descriptors is better.
	 */
	struct rlimit rlim;
	int rv = ::getrlimit(RLIMIT_NOFILE, &rlim);
	if (rv == 0) {
		if (rlim.rlim_cur < rlim.rlim_max) {
			rlim.rlim_cur = rlim.rlim_max;

			rv = ::setrlimit(RLIMIT_NOFILE, &rlim);
			if (rv == -1) {
				INFO(log_) << "Unable to increase file descriptor limit.";
			}
		}
	} else {
		INFO(log_) << "Unable to get file descriptor limit.";
	}
}

IOSystem::~IOSystem()
{
	ASSERT(log_, handle_map_.empty());
}

void
IOSystem::attach(int fd, Channel *owner)
{
	ASSERT(log_, handle_map_.find(handle_key_t(fd, owner)) == handle_map_.end());
	handle_map_[handle_key_t(fd, owner)] = new IOSystem::Handle(fd, owner);
}

void
IOSystem::detach(int fd, Channel *owner)
{
	handle_map_t::iterator it;
	IOSystem::Handle *h;

	it = handle_map_.find(handle_key_t(fd, owner));
	ASSERT(log_, it != handle_map_.end());

	h = it->second;
	ASSERT(log_, h != NULL);
	ASSERT(log_, h->owner_ == owner);

	handle_map_.erase(it);
	delete h;
}

Action *
IOSystem::close(int fd, Channel *owner, SimpleCallback *cb)
{
	IOSystem::Handle *h;

	h = handle_map_[handle_key_t(fd, owner)];
	ASSERT(log_, h != NULL);

	ASSERT(log_, h->close_callback_ == NULL);
	ASSERT(log_, h->close_action_ == NULL);

	ASSERT(log_, h->read_callback_ == NULL);
	ASSERT(log_, h->read_action_ == NULL);

	ASSERT(log_, h->write_callback_ == NULL);
	ASSERT(log_, h->write_action_ == NULL);

	ASSERT(log_, h->fd_ != -1);

	h->close_callback_ = cb;
	h->close_action_ = h->close_schedule();
	return (cancellation(h, &IOSystem::Handle::close_cancel));
}

Action *
IOSystem::read(int fd, Channel *owner, off_t offset, size_t amount, EventCallback *cb)
{
	IOSystem::Handle *h;

	h = handle_map_[handle_key_t(fd, owner)];
	ASSERT(log_, h != NULL);

	ASSERT(log_, h->read_callback_ == NULL);
	ASSERT(log_, h->read_action_ == NULL);

	/*
	 * Reads without an offset may be 0 length, but reads with
	 * an offset must have a specified length.
	 */
	ASSERT(log_, offset == -1 || amount != 0);

	/*
	 * If we have an offset, we must invalidate any outstanding
	 * buffers, since they are for data that may not be relevant
	 * to us.
	 */
	if (offset != -1) {
		h->read_buffer_.clear();
	}

	h->read_offset_ = offset;
	h->read_amount_ = amount;
	h->read_callback_ = cb;
	Action *a = h->read_do();
	ASSERT(log_, h->read_action_ == NULL);
	if (a == NULL) {
		ASSERT(log_, h->read_callback_ != NULL);
		h->read_action_ = h->read_schedule();
		ASSERT(log_, h->read_action_ != NULL);
		return (cancellation(h, &IOSystem::Handle::read_cancel));
	}
	ASSERT(log_, h->read_callback_ == NULL);
	return (a);
}

/* Notify when there is data ready to be read */
Action *
IOSystem::read_notify(int fd, Channel *owner, EventCallback *cb)
{

	IOSystem::Handle *h;

	h = handle_map_[handle_key_t(fd, owner)];
	ASSERT(log_, h != NULL);

	ASSERT(log_, h->read_callback_ == NULL);
	ASSERT(log_, h->read_action_ == NULL);

        h->read_action_ = h->read_schedule(cb);
        return (cancellation(h, &IOSystem::Handle::read_cancel));
	
}

Action *
IOSystem::write(int fd, Channel *owner, off_t offset, Buffer *buffer, EventCallback *cb)
{
	IOSystem::Handle *h;

	h = handle_map_[handle_key_t(fd, owner)];
	ASSERT(log_, h != NULL);

	ASSERT(log_, h->write_callback_ == NULL);
	ASSERT(log_, h->write_action_ == NULL);
	ASSERT(log_, h->write_buffer_.empty());

	ASSERT(log_, !buffer->empty());
	buffer->moveout(&h->write_buffer_);

	h->write_offset_ = offset;
	h->write_callback_ = cb;
	Action *a = h->write_do();
	ASSERT(log_, h->write_action_ == NULL);
	if (a == NULL) {
		ASSERT(log_, h->write_callback_ != NULL);
		h->write_action_ = h->write_schedule();
		ASSERT(log_, h->write_action_ != NULL);
		return (cancellation(h, &IOSystem::Handle::write_cancel));
	}
	ASSERT(log_, h->write_callback_ == NULL);
	return (a);
}
