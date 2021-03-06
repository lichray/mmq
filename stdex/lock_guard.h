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

template <typename... _Lockable>
struct lock_guard {
	lock_guard(_Lockable&... l) :
		locks(l...) {
		std::lock(l...);
	}

	lock_guard(_Lockable&... l, std::adopt_lock_t) :
		locks(l...) {}

	~lock_guard() {
		unlock(locks);
	}

	lock_guard(lock_guard const&) = delete;
	lock_guard& operator=(lock_guard const&) = delete;

private:
	template <typename... Lockable>
	static void unlock(std::tuple<Lockable&...>& locks) {
		unlock_impl<std::tuple_size<
			std::tuple<Lockable&...>>::value - 1>::apply(locks);
	}

	template <size_t N, typename Dummy = void>
	struct unlock_impl {
		template <typename... Lockable>
		void static apply(std::tuple<Lockable&...>& locks) {
			std::get<N>(locks).unlock();
			unlock_impl<N - 1>::apply(locks);
		}
	};

	template <typename Dummy>
	struct unlock_impl<0, Dummy> {
		template <typename... Lockable>
		void static apply(std::tuple<Lockable&...>& locks) {
			std::get<0>(locks).unlock();
		}
	};

	std::tuple<_Lockable&...> locks;
};

}

#endif
