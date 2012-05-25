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

#include <queue>
#include <stack>
#include <mutex>
#include <condition_variable>

namespace mmq {

template <typename _T>
struct identity { typedef _T type; };

using status = std::cv_status;

namespace policy {
	template <typename _Tp, typename _Rep>
	struct Policy {
		Policy() : _Impl() {}
		Policy(Policy const&) = delete;
		Policy& operator=(Policy const&) = delete;

		typedef typename _Rep::value_type value_type;
		typedef typename _Rep::const_reference const_reference;

		auto get() -> const_reference {
			return _get(identity<_Rep>());
		}
		
		void pop() {
			_Impl.pop();
		}

		void put(const_reference o) {
			_Impl.push(o);
		}

		void put(value_type&& o) {
			_Impl.push(std::move(o));
		}

		auto size() const -> typename _Rep::size_type {
			return _Impl.size();
		}

		bool empty() const {
			return _Impl.empty();
		}

	private:
		template <typename _T>
		auto _get(identity<_T>) -> const_reference {
			return _Impl.top();
		}

		auto _get(identity<std::queue<_Tp>>) -> const_reference {
			return _Impl.front();
		}

		_Rep _Impl;
	};

	template <typename _Tp>
	using Fifo = Policy<_Tp, std::queue<_Tp>>;

	template <typename _Tp>
	using Lifo = Policy<_Tp, std::stack<_Tp>>;

	template <typename _Tp>
	using Priority = Policy<_Tp, std::priority_queue<_Tp>>;
}

template <typename _Tp, template <typename _V> class _Repp = policy::Fifo>
struct Queue {
	explicit Queue(size_t maxsize) :
		maxsize(maxsize), unfinished_tasks() {}
	Queue() : Queue(0) {}
	Queue(Queue const&) = delete;
	Queue& operator=(Queue const&) = delete;

	typedef _Repp<_Tp> queue_type;
	typedef typename queue_type::value_type task_type;

	template <typename Func>
	void process(Func&& f) {
		task_type v;

		{
			std::unique_lock<std::mutex> lock(mutex);

			not_empty.wait(lock, [&]() {
				return not tasks.empty();
			});
			v = tasks.get();
			tasks.pop();
			if (maxsize)
				not_full.notify_one();
		}

		task_done _(*this);
		f(v);
	}

	template <typename Rep, typename Period, typename Func>
	status process(std::chrono::duration<Rep, Period> const& timeout,
	    Func&& f) {
		task_type v;

		{
			std::unique_lock<std::mutex> lock(mutex);

			bool got = not_empty.wait_for(lock, timeout, [&]() {
				return not tasks.empty();
			});
			if (not got)
				return status::timeout;
			v = tasks.get();
			tasks.pop();
			if (maxsize)
				not_full.notify_one();
		}

		task_done _(*this);
		f(v);

		return status::no_timeout;
	}

	void join() {
		std::unique_lock<std::mutex> lock(mutex);
		while (unfinished_tasks)
			all_tasks_done.wait(lock);
	}

	void put(task_type const& o) {
		std::unique_lock<std::mutex> lock(mutex);

		if (maxsize)
			not_full.wait(lock, [&]() {
				return tasks.size() != maxsize;
			});
		tasks.put(o);
		++unfinished_tasks;
		not_empty.notify_one();
	}

	void put(task_type&& o) {
		std::unique_lock<std::mutex> lock(mutex);

		if (maxsize)
			not_full.wait(lock, [&]() {
				return tasks.size() != maxsize;
			});
		tasks.put(std::move(o));
		++unfinished_tasks;
		not_empty.notify_one();
	}

	template <typename Rep, typename Period>
	status put(std::chrono::duration<Rep, Period> const& timeout,
	    task_type const& o) {
		std::unique_lock<std::mutex> lock(mutex);

		if (maxsize) {
			bool done = not_full.wait_for(lock, timeout, [&]() {
				return tasks.size() != maxsize;
			});
			if (not done)
				return status::timeout;
		}
		tasks.put(o);
		++unfinished_tasks;
		not_empty.notify_one();

		return status::no_timeout;
	}

	template <typename Rep, typename Period>
	status put(std::chrono::duration<Rep, Period>&& timeout,
	    task_type&& o) {
		std::unique_lock<std::mutex> lock(mutex);

		if (maxsize) {
			bool done = not_full.wait_for(lock, timeout, [&]() {
				return tasks.size() != maxsize;
			});
			if (not done)
				return status::timeout;
		}
		tasks.put(std::move(o));
		++unfinished_tasks;
		not_empty.notify_one();

		return status::no_timeout;
	}

private:
	struct task_done {
		task_done(Queue& o) : obj(o) {}
		~task_done() noexcept {
			std::lock_guard<std::mutex> _(obj.mutex);
			if (obj.unfinished_tasks == 1)
				obj.all_tasks_done.notify_all();
			--obj.unfinished_tasks;
		}
		Queue& obj;
	};

	const size_t maxsize;
	queue_type tasks;
	std::mutex mutex;
	std::condition_variable not_full;
	std::condition_variable not_empty;
	std::condition_variable all_tasks_done;
	size_t unfinished_tasks;
};

template <typename _Tp>
using LifoQueue = Queue<_Tp, policy::Lifo>;

template <typename _Tp>
using PriorityQueue = Queue<_Tp, policy::Priority>;

}
