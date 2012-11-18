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

#ifndef _STDEX_LOCK_GUARD_H
#define _STDEX_LOCK_GUARD_H 1

#include <mutex>
#include <tuple>

namespace stdex {

template <typename L1>
void unlock(L1& l1) {
	l1.unlock();
}

template <typename L1, typename... L2>
void unlock(L1& l1, L2&... l2) {
	l1.unlock();
	unlock<L2...>(l2...);
}

template <typename... _Lockable>
struct lock_guard {
	explicit lock_guard(_Lockable&... l) :
		locks(l...) {
		std::lock(l...);
	}

	~lock_guard() {
		unlock(std::get<0>(locks), std::get<1>(locks));
	}

	lock_guard(lock_guard const&) = delete;
	lock_guard& operator=(lock_guard const&) = delete;

private:
	std::tuple<_Lockable&...> locks;
};

}

#endif
