/*!
 * \file main.cpp
 * \brief Threaded program to find every likely primes in big values intervals.
 * \author Vincent Commin & Louis Leenart
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gmpxx.h>

#include "Chrono.hpp"
#include "miller-rabin-gmp.hpp"


void * compute_prime_2_worker(void * data) {
	// Retrieve data from master
	struct thread_data_2 * tdi = (struct thread_data_2 *) data;
	// Local primes found by the worker. To be merge with tdi.primes when the worker is done
	std::vector<mpz_class> worker_primes = {};
	gmp_randclass *rnd = initialize_seed();

	// While the are intervals to process
	while (true) {
		// Take a new interval of values, if no more intervals, stop.
		pthread_mutex_lock(&mutex_index);
		if (tdi->index == -1) {
			pthread_mutex_unlock(&mutex_index);
			break;
		}
		// Take the new interval
		mpz_class from, to;
		try {
			from = tdi->intervals->at(tdi->index);
			to = tdi->intervals->at(tdi->index + 1);
		} catch (std::out_of_range & oor) {
			tdi->index = -1;
			pthread_mutex_unlock(&mutex_index);
			break;
		}
		// Bump index for other workers
		tdi->index+=2;
		pthread_mutex_unlock(&mutex_index);
		
		// Process each value in the interval
		for (mpz_class i = from; mpz_cmp(i.get_mpz_t(), to.get_mpz_t()) < 0; i++) {
			if (prob_prime(i, tdi->rounds, rnd)) { // If the number is likely prime, keep it
				worker_primes.push_back(i);
			}
		}
	} 

	// Merge local primes with tdi.primes
	if (worker_primes.size() > 0) {
		pthread_mutex_lock(&mutex_primes);
		tdi->primes->insert(tdi->primes->end(), worker_primes.begin(), worker_primes.end());
		pthread_mutex_unlock(&mutex_primes);
	}
	delete(rnd);
	pthread_exit(EXIT_SUCCESS);
}

/*
* Find every (likely) primes in the `intervals`, threaded on `nb_threads`.
* intervals : vector of values representing intervals, [lower_bound1, upper_bound1, lower_bound2,
* upper_bound2, ...].
* rounds : number of miller-rabin approximation rounds, the higher the more precision, but the more
* compute time.
* nb_threads : number of parallel threads launched.
*
* return : vector of unordered likely primes found in the intervals. The pointer needs to be deleted
* by the caller. 
*
* This function relies on `compute_prime_2_worker` function.
*/
std::vector<mpz_class>* compute_prime(std::vector<std::pair<mpz_class, mpz_class>> * intervals, int rounds, int nb_threads) {
	// Init result vector and mutex
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;

	// Launch threads
	for (int i = 0; i < nb_threads; i++)
		pthread_create(&ids[i], NULL, &compute_prime_2_worker, &tdi);
	
	// Wait for every threads to finish
	for (int i = 0; i < nb_threads; i++)
		pthread_join(ids[i], NULL);
	
	return primes; // property of caller
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
    if (argc >= 4)
        rounds = atoi(argv[3]);
    
	/* Read input file
	 * Expected format is the following :
	 * A B
	 * C D
	 * ...
	 * Meaning intervals are : 
	 * [[A, B], [C, D], ...] 
	 */
	std::ifstream file;
	file.open(argv[2]);
	if (file.is_open()) {
	
		std::vector<std::pair<mpz_class, mpz_class>> * intervals = new std::vector<std::pair<mpz_class, mpz_class>>();
		std::string line;

		// For each line, parse to find substring<space>substring, 
		// then tries to parse each substring into long integers (base10)
		// represented by GMP as a mpz_class type.
		while (getline(file, line)) {
			std::pair<mpz_class, mpz_class> p;
			std::istringstream iss(line);
			std::string val;
			iss >> val;
			p.first = mpz_class(val);
			iss >> val;
			p.second = mpz_class(val);
			intervals->push_back(p);
		}
		file.close();

		// Vector of found likely primes in intervals
		std::vector<mpz_class> * primes;
		// Compute time
		Chrono c(true);
		// Launch computation for every intervals
		primes = compute_prime(intervals, rounds, nb_thread);
		c.pause();
		// Print every found likely primes in order
		std::sort(primes->begin(), primes->end());
		for (mpz_class p : *primes) {
			std::cout << p << " ";
		}
		std::cout << std::endl;
		// Time to compute
		std::cerr << c.get() << std::endl;
		
		delete(intervals);
		delete(primes);
	} else {
		std::cerr << "error: can\'t open file at : " << argv[2] << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}