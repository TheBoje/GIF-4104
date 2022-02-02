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
	int rounds;
};

pthread_mutex_t mutex_vec;
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
			pthread_mutex_lock(&mutex_vec);
			td->primes->push_back(target);
			pthread_mutex_unlock(&mutex_vec);
		}
	}
  	pthread_exit(EXIT_SUCCESS);
}

void * compute_prime_worker_improved(void * data) {
	struct thread_data_improved * tdi = (struct thread_data_improved *) data;
	std::vector<mpz_class> * worker_primes = new std::vector<mpz_class>;
	gmp_randclass *rnd = initialize_seed();

	for (;;) {
		pthread_mutex_lock(&mutex_intervals);
		if (tdi->intervals->size() < 2) {
			pthread_mutex_unlock(&mutex_intervals);
			return worker_primes;
		}
		mpz_class from = tdi->intervals->at(0);
		tdi->intervals->erase(tdi->intervals->begin());
		mpz_class to = tdi->intervals->at(0);
		tdi->intervals->erase(tdi->intervals->begin());
		pthread_mutex_unlock(&mutex_intervals);

		for (mpz_class i = from; mpz_cmp(i.get_mpz_t(), to.get_mpz_t()) < 0; i++) {
			if (prob_prime(i, tdi->rounds, rnd)) {
				worker_primes->push_back(i);
			}
		}
	} 
}

double compute_prime(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	std::cout << "compute_prime" << std::endl;
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	Chrono c(false);
	while (intervals->size() > 0) {
		// Declared threads and  Init mutex
		pthread_t ids[nb_threads];
		
		pthread_mutex_init(&mutex_count, NULL);
		pthread_mutex_init(&mutex_vec, NULL);

		// Create data structure shared by all the threads
		struct thread_data td;
		td.rounds = rounds;
		td.count = intervals->at(0);
		intervals->erase(intervals->begin());
		td.max = intervals->at(0);
		intervals->erase(intervals->begin());
		td.primes = primes;
		td.rnd = initialize_seed();

		c.resume();

		// Run the threads, each thread will pick a number in the interval when he is ready
		for(int i = 0; i < nb_threads; i++)
			pthread_create(&ids[i], NULL, &compute_prime_worker, &td);

		for(int i = 0; i < nb_threads; i++) {
			pthread_join(ids[i], NULL);
		}

		c.pause();

	}
	std::sort(primes->begin(), primes->end());
	// for (mpz_class i : *primes) {
	// 	std::cout << i << " ";
	// }
	// std::cout << std::endl;
	std::cout << "count: " << primes->size() << std::endl;
    delete(primes);
	return c.get();
}

double compute_prime_improved(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	std::cout << "compute_prime_improved" << std::endl;
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	std::vector<mpz_class> * from_workers[nb_threads];
	pthread_mutex_init(&mutex_intervals, NULL);
	pthread_t ids[nb_threads];
	struct thread_data_improved tdi;
	tdi.intervals = intervals;
	tdi.rounds = rounds;

	Chrono c(true);

	for (int i = 0; i < nb_threads; i++) {
		pthread_create(&ids[i], NULL, &compute_prime_worker_improved, &tdi);
	}

	for (int i = 0; i < nb_threads; i++) {
		pthread_join(ids[i], (void **)&(from_workers[i]));
	}

	c.pause(); // TODO stop now or after merging primes ?

	// Merge from_workers array of primes (which is a vector of mpz_class), into `primes`
	for (int i = 0; i < nb_threads; i++) {
		primes->insert(primes->end(), from_workers[i]->begin(), from_workers[i]->end());
		delete(from_workers[i]);
	}

	std::sort(primes->begin(), primes->end());
	// for (mpz_class p : *primes) {
	// 	std::cout << p << " ";
	// }
	// std::cout << std::endl;
	std::cout << "count: " << primes->size() << std::endl;
	delete(primes);
	return c.get();
}

int main(int argc, char** argv) {
	// Parse args
	if (argc < 3) {
		std::cerr << "usage: executable <nb_threads> <filepath> [rounds]" << std::endl; 
		return EXIT_FAILURE;
	}
	unsigned int rounds = 10;
	if (argc >= 4) {
		rounds = atoi(argv[3]);
	}
	unsigned int nb_thread = atoi(argv[1]);

	/* Read input file
	 * Expected format is the following :
	 * A B
	 * C D
	 * ... 
	 */
	std::ifstream file;
	file.open(argv[2]);
	// int line_nb = 0;
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
		// double total_time = compute_prime(intervals, rounds, nb_thread);
		double total_time = compute_prime_improved(intervals, rounds, nb_thread);
		std::cerr << total_time << " sec total" << std::endl;
		delete(intervals);
	} else {
		std::cerr << "error: can\'t open file at : " << argv[2] << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}