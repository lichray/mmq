/*
 * A toy program that lists the users on your machine (buggy),
 * one second per user.
 */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <vector>

#include "mmq/Queue.h"

using std::string;

void consumer(std::atomic_bool const& done, mmq::Queue<string>& q) {
	using namespace std;
	while (not done.load(std::memory_order_relaxed))
		q.process(chrono::seconds(2), [](string& v) {
			auto len_uname = v.find(':');
			if (v.empty() or v.at(0) == '#'
			    or len_uname == string::npos)
				return;
			v.resize(len_uname);
			this_thread::sleep_for(chrono::seconds(1));
			flockfile(stdout);
			cout << this_thread::get_id()
			     << ' ' << v << endl;
			funlockfile(stdout);
		});
}

int main() {
	mmq::Queue<string> ls(6);
	string ln;
	std::ifstream fp("/etc/passwd");
	std::vector<std::thread> pool;
	std::atomic_bool flag(false);

	for (size_t i = 0;
	    i < std::thread::hardware_concurrency(); ++i) {
		std::thread t(consumer, cref(flag), std::ref(ls));
		pool.push_back(std::move(t));
	}
	while (getline(fp, ln))
		ls.put(std::chrono::seconds(4), ln);

	ls.join();
	std::cerr << " -- queue joined --\n";
	flag.store(true, std::memory_order_relaxed);
	for (auto& t : pool)
		t.join();
	return 0;
}
