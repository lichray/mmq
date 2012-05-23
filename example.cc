/*
 * A toy program that lists the users on your machine (buggy),
 * one second per user.
 */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>

#include "mmq.h"

using std::string;

void consumer(int& done, mmq::Queue<string>& q) {
	using namespace std;
	while (not done)
		q.process([](string& v) {
			size_t len_uname = v.find(':');
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
	int flag = 0;

	for (size_t i = 0;
	    i < std::thread::hardware_concurrency(); ++i) {
		std::thread t(consumer, std::ref(flag), std::ref(ls));
		pool.push_back(std::move(t));
	}
	while (std::getline(fp, ln))
		ls.put(ln);

	ls.join();
	std::cerr << " -- queue joined --\n";
	flag = 1;
	for (std::thread& t : pool)
		t.join();
	return 0;
}
