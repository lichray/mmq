#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <unistd.h>

#include <mutex>
#include <ext/mutex_sem.h>

template <typename Mutex>
void calc(Mutex& mu, uint64_t& var, size_t& times) {
	while (1) {
		std::lock_guard<Mutex> _(mu);
		if (!times)
			break;
		var = (var * 33) + 17;
		--times;
	}
}

int main(int argc, char* argv[]) {
	std::vector<std::thread> pool;
	bool use_sem = 0;
	uint64_t var = 1;
	size_t times = 0;
	size_t nthrs = std::thread::hardware_concurrency();
	stdex::mutex_sem mu_sem;
	std::mutex mu;

	int ch;
	while ((ch = getopt(argc, argv, "sj:n:")) != -1) {
		switch (ch) {
		case 's':
			use_sem = 1;
			break;
		case 'j':
			nthrs = strtoul(optarg, NULL, 10);
			break;
		case 'n':
			times = strtoul(optarg, NULL, 10);
			break;
		default:
			printf("%s [-s] [-j jobs] [-n times]\n", argv[0]);
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	if (use_sem)
		for (size_t i = 0; i < nthrs; ++i) {
			std::thread t(calc<decltype(mu_sem)>,
			    std::ref(mu_sem), std::ref(var), std::ref(times));
			pool.push_back(std::move(t));
		}
	else
		for (size_t i = 0; i < nthrs; ++i) {
			std::thread t(calc<decltype(mu)>,
			    std::ref(mu), std::ref(var), std::ref(times));
			pool.push_back(std::move(t));
		}

	for (auto& t : pool)
		t.join();

	return 0;
}
