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

namespace policy {
	template <template <typename... Args> class _Repp, typename _Tp>
	class Policy {
		typedef _Repp<_Tp> _Rep;

	public:
		Policy() : _Impl() {}
		Policy(Policy const&) = delete;
		Policy& operator=(Policy const&) = delete;

		typedef typename _Rep::value_type value_type;

		value_type&& get() {
			force_pop _(_Impl);
			return std::move(_get(identity<_Rep>()));
		}

		void put(value_type const& o) {
			_Impl.push(o);
		}

		void put(value_type&& o) {
			_Impl.push(std::move(o));
		}

		typename _Rep::size_type size() const {
			return _Impl.size();
		}

		bool empty() const {
			return _Impl.empty();
		}

	private:
		struct force_pop {
			force_pop(_Rep& o) : obj(o) {}
			~force_pop() noexcept {
				obj.pop();
			}
			_Rep& obj;
		};

		template <typename _T>
		value_type&& _get(identity<_T>) {
			return std::move(_Impl.top());
		}

		value_type&& _get(identity<std::queue<_Tp>>) {
			return std::move(_Impl.front());
		}

		_Rep _Impl;
	};

	template <typename _Tp>
	using Fifo = Policy<std::queue, _Tp>;

	template <typename _Tp>
	using Lifo = Policy<std::stack, _Tp>;

	template <typename _Tp>
	using Priority = Policy<std::priority_queue, _Tp>;
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
	void process(Func const& f) {
		task_type v;

		{
			std::unique_lock<std::mutex> lock(mutex);

			not_empty.wait(lock, [&]() {
				return not tasks.empty();
			});
			v = std::move(tasks.get());
			if (maxsize)
				not_full.notify_one();
		}

		task_done _(*this);
		f(v);
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
