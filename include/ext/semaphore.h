/*-
 * Copyright (c) 2012 Zhihao Yuan.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H 1

#include <semaphore.h>
#include <condition_variable> /* for std::cv_status */

namespace stdex {

using sem_status = std::cv_status;

class semaphore {
	typedef sem_t		native_type;

public:
	typedef native_type*	native_handle_type;

	explicit semaphore(unsigned int value);
	semaphore() : semaphore(0) {}
	~semaphore() noexcept;

	semaphore(semaphore const&) = delete;
	semaphore& operator=(semaphore const&) = delete;

	void notify() {
		sem_post(sem);
	}

	void wait() {
		sem_wait(sem);
	}

	template <typename Pred>
	void wait(Pred f) {
		while (!f())
			wait();
	}

	bool try_wait() {
		return sem_trywait(sem) == 0;
	}

	auto value() -> int {
		int v;
		sem_getvalue(sem, &v);
		return v;
	}
	
	auto native_handle() -> native_handle_type {
		return sem;
	}

private:
	native_type sem[1];
};

void __throw_system_error(int ev, const char* what_arg);

}

#endif
