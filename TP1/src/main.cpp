#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <pthread.h>
#include <gmpxx.h>

#include "Chrono.hpp"
#include "miller-rabin-gmp.hpp"

struct thread_data {
	std::vector<mpz_class> * primes;
	gmp_randclass *rnd;
  	int rounds;
  	mpz_class count;
  	mpz_class max;
};

struct thread_data_improved {
	std::vector<mpz_class> * intervals;
	std::vector<mpz_class> * primes;
	int rounds;
};

pthread_mutex_t mutex_primes;
pthread_mutex_t mutex_count;
pthread_mutex_t mutex_intervals;

void * compute_prime_worker(void * data) {
  	struct thread_data * td = (struct thread_data *)data;
	for (;;) {
		// Get the actual count and verify if it is not above the up part of the interval
		pthread_mutex_lock(&mutex_count);
		if (td->count >= td->max) {pthread_mutex_unlock(&mutex_count); break;}
		mpz_class target = td->count;
		td->count++;
		pthread_mutex_unlock(&mutex_count);
		// Run the Miller-Rabin algorithm  
		bool res = prob_prime(target, td->rounds, td->rnd);

		// if the number is considered as prime, write it in the result vector
		if (res) {
			pthread_mutex_lock(&mutex_primes);
			td->primes->push_back(target);
			pthread_mutex_unlock(&mutex_primes);
		}
	}
  	pthread_exit(EXIT_SUCCESS);
}

void * compute_prime_worker_improved(void * data) {
	struct thread_data_improved * tdi = (struct thread_data_improved *) data;
	std::vector<mpz_class> worker_primes = {};
	gmp_randclass *rnd = initialize_seed();

	while (true) {
		pthread_mutex_lock(&mutex_intervals);
		if (tdi->intervals->size() < 2) {
			pthread_mutex_unlock(&mutex_intervals);
            delete(rnd);
			pthread_exit(EXIT_SUCCESS);
		}
		mpz_class from = tdi->intervals->at(0);
		mpz_class to = tdi->intervals->at(1);
		tdi->intervals->erase(tdi->intervals->begin(), tdi->intervals->begin() + 1);
		pthread_mutex_unlock(&mutex_intervals);

		for (mpz_class i = from; mpz_cmp(i.get_mpz_t(), to.get_mpz_t()) < 0; i++) {
			if (prob_prime(i, tdi->rounds, rnd)) {
				worker_primes.push_back(i);
			}
		}

		if (worker_primes.size() > 0) {
			pthread_mutex_lock(&mutex_primes);
			tdi->primes->insert(tdi->primes->end(), worker_primes.begin(), worker_primes.end());
			pthread_mutex_unlock(&mutex_primes);
		}
	} 
}

std::vector<mpz_class>* compute_prime(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	pthread_mutex_init(&mutex_count, NULL);
	pthread_mutex_init(&mutex_primes, NULL);
	// Declared threads and  Init mutex
	pthread_t ids[nb_threads];

	while (intervals->size() > 0) {
		// Create data structure shared by all the threads
		struct thread_data td;
		td.rounds = rounds;
		td.count = intervals->at(0);
		intervals->erase(intervals->begin());
		td.max = intervals->at(0);
		intervals->erase(intervals->begin());
		td.primes = primes;
		td.rnd = initialize_seed();

		// Run the threads, each thread will pick a number in the interval when he is ready
		for(int i = 0; i < nb_threads; i++)
			pthread_create(&ids[i], NULL, &compute_prime_worker, &td);

		for(int i = 0; i < nb_threads; i++) {
			pthread_join(ids[i], NULL);
		}
	}
	std::sort(primes->begin(), primes->end());
    return primes;
}

std::vector<mpz_class>* compute_prime_improved(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	pthread_mutex_init(&mutex_intervals, NULL);
	pthread_mutex_init(&mutex_primes, NULL);
	pthread_t ids[nb_threads];

	struct thread_data_improved tdi{};
	tdi.intervals = intervals;
	tdi.rounds = rounds;
	tdi.primes = primes;

	for (int i = 0; i < nb_threads; i++) {
		pthread_create(&ids[i], NULL, &compute_prime_worker_improved, &tdi);
	}

	for (int i = 0; i < nb_threads; i++) {
		pthread_join(ids[i], NULL);
	}

	std::sort(primes->begin(), primes->end());

	return primes;
}

int main(int argc, char** argv) {
	// Parse args
	if (argc < 3) {
		std::cerr << "usage: executable <nb_threads> <filepath> [rounds]" << std::endl; 
		return EXIT_FAILURE;
	}
	unsigned int rounds = 5;
	unsigned int nb_thread;

    nb_thread = atoi(argv[1]);
    if (argc >= 4) {
        rounds = atoi(argv[3]);
    }

	

	/* Read input file
	 * Expected format is the following :
	 * A B
	 * C D
	 * ... 
	 */
	std::ifstream file;
	file.open(argv[2]);
	if (file.is_open()) {
		std::vector<mpz_class> * intervals = new std::vector<mpz_class>();
		std::string line;
		while (getline(file, line)) {
			std::istringstream iss(line);
			std::string val;
			iss >> val;
			intervals->push_back(mpz_class(val));
			iss >> val;
			intervals->push_back(mpz_class(val));
		}
		file.close();

		// Total processing time of prime-computing only
		std::vector<mpz_class> * primes;
		Chrono c(true);
		// primes = compute_prime(intervals, rounds, nb_thread);
		primes = compute_prime_improved(intervals, rounds, nb_thread);
		c.pause();
		for (mpz_class p : *primes) {
			std::cout << p << " ";
		}
		std::cout << std::endl;
		std::cerr << c.get() << std::endl;
		delete(intervals);
		delete(primes);
	} else {
		std::cerr << "error: can\'t open file at : " << argv[2] << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}