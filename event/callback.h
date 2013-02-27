/*
 * Copyright (c) 2008-2012 Juli Mallett. All rights reserved.
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

#ifndef	EVENT_CALLBACK_H
#define	EVENT_CALLBACK_H

#include <event/action.h>

class CallbackBase;

class CallbackScheduler {
protected:
	CallbackScheduler(void)
	{ }

public:
	virtual ~CallbackScheduler()
	{ }

	virtual Action *schedule(CallbackBase *) = 0;
};

class CallbackBase {
	CallbackScheduler *scheduler_;
protected:
	CallbackBase(CallbackScheduler *scheduler)
	: scheduler_(scheduler)
	{ }

public:
	virtual ~CallbackBase()
	{ }

public:
	virtual void execute(void) = 0;

	Action *schedule(void);
};

class SimpleCallback : public CallbackBase {
protected:
	SimpleCallback(CallbackScheduler *scheduler)
	: CallbackBase(scheduler)
	{ }

public:
	virtual ~SimpleCallback()
	{ }

	void execute(void)
	{
		(*this)();
	}

protected:
	virtual void operator() (void) = 0;
};

#endif /* !EVENT_CALLBACK_H */
