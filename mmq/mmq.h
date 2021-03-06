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

#ifndef _MMQ_MMQ_H
#define _MMQ_MMQ_H 1

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <stdex/lock_guard.h>

namespace mmq {

using status = std::cv_status;
using std::swap;

namespace policy {
	template <typename _Tp, typename _Rep>
	struct Policy {
		Policy() = default;
		Policy(Policy const&) = delete;
		Policy& operator=(Policy const&) = delete;

		Policy(Policy&& q) noexcept(
		    std::is_nothrow_move_constructible<_Rep>::value) :
			impl_(std::move(q.impl_)) {}

		Policy& operator=(Policy&& q) noexcept(
		    noexcept(Policy().swap(q))) {
			swap(q);
			return *this;
		}


		typedef typename _Rep::value_type	value_type;
		typedef typename _Rep::reference	reference;
		typedef typename _Rep::size_type	size_type;

		auto get() -> reference {
			return _get(identity<_Rep>());
		}
		
		void pop() {
			impl_.pop();
		}

		template <typename T>
		void push(T o) {
			impl_.push(std::move(o));
		}

		auto size() const -> size_type {
			return impl_.size();
		}

		bool empty() const {
			return impl_.empty();
		}

		void swap(Policy& o) noexcept(
		    noexcept(swap(std::declval<Policy&>().impl_, o.impl_))) {
			using std::swap;
			swap(impl_, o.impl_);
		}

	private:
		template <typename _T>
		struct identity {
			typedef _T type;
		};

		/* Tricky. Don't try this at home. */
		template <typename _T>
		auto _get(identity<_T>) -> reference {
			return impl_.top();
		}

		auto _get(identity<std::priority_queue<_Tp>>) -> reference {
			return const_cast<reference>(impl_.top());
		}

		auto _get(identity<std::queue<_Tp>>) -> reference {
			return impl_.front();
		}

		_Rep impl_;
	};

	template <typename _Tp, typename _Rep>
	inline void swap(Policy<_Tp, _Rep>& a, Policy<_Tp, _Rep>& b) noexcept(
	    noexcept(a.swap(b))) {
		a.swap(b);
	}

	template <typename _Tp>
	using Fifo = Policy<_Tp, std::queue<_Tp>>;
}

template <typename _Tp, template <typename> class _Rep_ = policy::Fifo>
struct Queue {
	typedef _Rep_<_Tp>			queue_type;
	typedef typename queue_type::value_type	task_type;
	typedef typename queue_type::size_type	size_type;

	explicit Queue(size_type maxsize) :
		maxsize(maxsize), unfinished_tasks(), cv(new cv_impl) {}
	Queue() : Queue(0) {}
	Queue(Queue const&) = delete;
	Queue& operator=(Queue const&) = delete;

	Queue(Queue&& q) noexcept(
	    std::is_nothrow_move_constructible<decltype(q)>::value) :
		Queue(std::move(q), std::lock_guard<std::mutex>(q.mutex)) {}

	Queue& operator=(Queue&& q) noexcept(
	    noexcept(Queue().swap(q))) {
		swap(q);
		return *this;
	}

	void reserve() {
		tasks.reserve(maxsize);
	}

	void reserve(size_type n) {
		tasks.reserve(n);
	}

	template <typename Func>
	void process(Func f) {
		task_type v;

		{
			std::unique_lock<std::mutex> lock(mutex);

			cv->not_empty.wait(lock, [&]() {
				return not tasks.empty();
			});
			using std::swap;
			swap(v, tasks.get());
			tasks.pop();
			if (maxsize)
				cv->not_full.notify_one();
		}

		task_done _(*this);
		f(v);
	}

	template <typename Rep, typename Period, typename Func>
	status process(std::chrono::duration<Rep, Period> const& timeout,
	    Func f) {
		task_type v;

		{
			std::unique_lock<std::mutex> lock(mutex);

			bool got = cv->not_empty.wait_for(lock, timeout, [&]() {
				return not tasks.empty();
			});
			if (not got)
				return status::timeout;
			using std::swap;
			swap(v, tasks.get());
			tasks.pop();
			if (maxsize)
				cv->not_full.notify_one();
		}

		task_done _(*this);
		f(v);

		return status::no_timeout;
	}

	void join() {
		std::unique_lock<std::mutex> lock(mutex);
		cv->all_tasks_done.wait(lock, [&]() {
			return unfinished_tasks == 0;
		});
	}

	template <typename T>
	void put(T o) {
		std::unique_lock<std::mutex> lock(mutex);

		if (maxsize)
			cv->not_full.wait(lock, [&]() {
				return tasks.size() != maxsize;
			});
		tasks.push(std::move(o));
		++unfinished_tasks;
		cv->not_empty.notify_one();
	}

	template <typename Rep, typename Period, typename T>
	status put(std::chrono::duration<Rep, Period> const& timeout, T o) {
		std::unique_lock<std::mutex> lock(mutex);

		if (maxsize) {
			bool done = cv->not_full.wait_for(lock, timeout, [&]() {
				return tasks.size() != maxsize;
			});
			if (not done)
				return status::timeout;
		}
		tasks.push(std::move(o));
		++unfinished_tasks;
		cv->not_empty.notify_one();

		return status::no_timeout;
	}

	void swap(Queue& q) noexcept(
	    noexcept(swap(std::declval<Queue&>().tasks, q.tasks))) {
		stdex::lock_guard<std::mutex, std::mutex> _(mutex, q.mutex);

		using std::swap;
		// no need to swap mutex
		swap(tasks, q.tasks);
		swap(maxsize, q.maxsize);
		swap(unfinished_tasks, q.unfinished_tasks);
		swap(cv, q.cv);
	}

private:
	struct task_done {
		task_done(Queue& o) : obj(o) {}
		~task_done() noexcept {
			std::lock_guard<std::mutex> _(obj.mutex);
			if (obj.unfinished_tasks == 1)
				obj.cv->all_tasks_done.notify_all();
			--obj.unfinished_tasks;
		}
		Queue& obj;
	};

	Queue(Queue&& q, std::lock_guard<std::mutex> const& _) :
		tasks(std::move(q.tasks)),
		maxsize(std::move(q.maxsize)),
		unfinished_tasks(std::move(q.unfinished_tasks)),
		cv(std::move(q.cv)) {}

	// not really pimpl; make cv swappable only.
	struct cv_impl {
		std::condition_variable not_full;
		std::condition_variable not_empty;
		std::condition_variable all_tasks_done;
	};

	std::mutex mutex;
	queue_type tasks;
	size_type maxsize;
	size_type unfinished_tasks;
	std::unique_ptr<cv_impl> cv;
};

template <typename _Tp, template <typename> class _Rep_>
inline void swap(Queue<_Tp, _Rep_>& a, Queue<_Tp, _Rep_>& b) noexcept(
    noexcept(a.swap(b))) {
	a.swap(b);
}

}

#endif
