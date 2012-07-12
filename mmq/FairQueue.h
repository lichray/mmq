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

#ifndef _MMQ_FAIRQUEUE_H
#define _MMQ_FAIRQUEUE_H 1

#include <random>
#include "mmq.h"

namespace mmq {

namespace policy {
	template <typename _Tp>
	class Random {
		typedef std::vector<_Tp>		_Rep;

	public:
		Random() : _impl() {}
		Random(Random const&) = delete;
		Random& operator=(Random const&) = delete;

		typedef typename _Rep::value_type	value_type;
		typedef typename _Rep::const_reference	const_reference;
		typedef typename _Rep::size_type	size_type;

		void reserve(size_type n) {
			_impl.reserve(n);
		}

		auto get() -> const_reference {
			typedef std::uniform_int_distribution<size_type> _Dp;
			typedef typename _Dp::param_type _Pp;
			_Dp dist;
			static std::mt19937 rng;

			auto i = dist(rng, _Pp(0, size() - 1));
			if (i != size() - 1)
				std::swap(_impl[i], _impl.back());
			return _impl.back();
		}
		
		void pop() {
			_impl.pop_back();
		}

		void push(const_reference o) {
			_impl.push_back(o);
		}

		void push(value_type&& o) {
			_impl.push_back(std::move(o));
		}

		auto size() const -> size_type {
			return _impl.size();
		}

		bool empty() const {
			return _impl.empty();
		}

	private:
		_Rep _impl;
	};
}

template <typename _Tp>
using FairQueue = Queue<_Tp, policy::Random>;

}

#endif
