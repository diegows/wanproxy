#ifndef	IO_PIPE_PAIR_PRODUCER_H
#define	IO_PIPE_PAIR_PRODUCER_H

#include <io/pipe/pipe_producer.h>
#include <io/pipe/pipe_producer_wrapper.h>

class PipePairProducer : public PipePair {
	PipeProducerWrapper<PipePairProducer> *incoming_pipe_;
	PipeProducerWrapper<PipePairProducer> *outgoing_pipe_;
protected:
	PipePairProducer(void)
	: incoming_pipe_(NULL),
	  outgoing_pipe_(NULL)
	{ }
public:
	virtual ~PipePairProducer()
	{
		if (incoming_pipe_ != NULL) {
			delete incoming_pipe_;
			incoming_pipe_ = NULL;
		}

		if (outgoing_pipe_ != NULL) {
			delete outgoing_pipe_;
			outgoing_pipe_ = NULL;
		}
	}

protected:
	virtual void incoming_consume(Buffer *) = 0;
	virtual void outgoing_consume(Buffer *) = 0;

	void incoming_produce(Buffer *buf)
	{
		incoming_pipe_->produce(buf);
	}

	void outgoing_produce(Buffer *buf)
	{
		outgoing_pipe_->produce(buf);
	}

public:
	Pipe *get_incoming(void)
	{
		ASSERT(incoming_pipe_ == NULL);
		incoming_pipe_ = new PipeProducerWrapper<PipePairProducer>(this, &PipePairProducer::incoming_consume);
		return (incoming_pipe_);
	}

	Pipe *get_outgoing(void)
	{
		ASSERT(outgoing_pipe_ == NULL);
		outgoing_pipe_ = new PipeProducerWrapper<PipePairProducer>(this, &PipePairProducer::outgoing_consume);
		return (outgoing_pipe_);
	}
};

#endif /* !IO_PIPE_PAIR_PRODUCER_H */
