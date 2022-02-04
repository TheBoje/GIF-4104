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

#include <pthread.h>
#include <gmpxx.h>

#include "Chrono.hpp"
#include "miller-rabin-gmp.hpp"

/* 
* Data shared from `compute_prime` to each `compute_prime_worker` thread.
* primes : return vector of values. Each thread add their found primes (with mutex `mutex_primes`).
* rnd : random number generator needed by miller rabin algorithm. Share amoung threads, prevents
* from re-allocating it each time. Thread safe (see GMP documentation). Initialised from
* `initialize_seed()`.
* rounds : number of rounds of miller-rabin algorithm to do. The higher the more accurate the result
* is, but the more expensive (time) it is.
* count : status of the computation. When a thread is free, it can read `count` as the next "target"
* to check if the number is prime or not, and increment it. The next thread will do the same until 
* `count` >= `max`. `count` initial value is the lower bound of the interval. `count` reads & writes
* requires the possession of the mutex `mutex_count`.
* max : upper bound of the interval. Read only (no mutex).
*/
struct thread_data {
	std::vector<mpz_class> * primes;
	gmp_randclass *rnd;
  	int rounds;
  	mpz_class count;
  	mpz_class max;
};


/*
* Data shared from `compute_prime_improved` to each `compute_prime_improved_worker` thread.
* intervals : stack of intervals. Shared amoung every worker, it need to be accessed (read & write)
* with a mutex (`mutex_intervals`). When a thread is free, it take the 2 first values (as lower and
* upper bounds of an interval) and can compute primes from them.
* primes : return vector of values. Shared amoung every worker, it need to be accessed (read &
* write) with a mutex (`mutex_primes`). When the stack of job is empty, before stopping the worker
* will take the mutex and write his found primes into `primes`. Initialised as empty.
*/
struct thread_data_improved {
	std::vector<mpz_class> * intervals;
	std::vector<mpz_class> * primes;
	int rounds;
};

// To read/write thread_data.primes or thread_data_improved.primes
pthread_mutex_t mutex_primes;
// To read/write thread_data.count
pthread_mutex_t mutex_count;
// To read/write thread_data.intervals or thread_data_improved.intervals
pthread_mutex_t mutex_intervals;

/*
* Thread function to find every primes between two mpz_class values.
* data : `thread_data` pointeur
* Process thread_data.count to thread_data.max values, push each potential prime value into
* thread_data.prime until there is no more values to test. 
* Requires mutex_primes & mutex_count initialised.
*
* NOTE: This is the first approch we took to the problem. As explained in the README.md, this method
* is not the most performant as we need to create then join thread for each intervals. This is works
* well for few intervals, but relies on many sys calls in the long run.
*/
void * compute_prime_worker(void * data) {
	// Retrieve data provided from master
  	struct thread_data * td = (struct thread_data *)data;
	std::vector<mpz_class> worker_primes{};
	// While there is numbers to test
	for (;;) {
		// Check if the remaining interval needs processing
		pthread_mutex_lock(&mutex_count);
		if (td->count >= td->max) {pthread_mutex_unlock(&mutex_count); break;} // no more values to test, closing thread.
		mpz_class target = td->count;
		td->count++;
		pthread_mutex_unlock(&mutex_count);
		// Run the Miller-Rabin algorithm  
		bool res = prob_prime(target, td->rounds, td->rnd);

		// if the number is (likely) prime, write it in the result vector
		/* 
		 * NOTE: this creates a lot of mutex system calls, maybe creating a local vector of found
		 * primes and dumping this vector at the end of the worker's life would only create 1 system
		 * call - but this double memory usage.
		 */
		if (res) {
			worker_primes.push_back(target);
			pthread_mutex_lock(&mutex_primes);
			td->primes->push_back(target);
			pthread_mutex_unlock(&mutex_primes);
		}
	}

	// Merge found likely prime into shared vector (need to wait for mutex)
	if (worker_primes.size() > 0) {
		pthread_mutex_lock(&mutex_primes);
		td->primes->insert(td->primes->end(), worker_primes.begin(), worker_primes.end());
		pthread_mutex_unlock(&mutex_primes);

	}

  	pthread_exit(EXIT_SUCCESS);
}

/*
* Thread function to find every primes between two mpz_class values.
* data : `thread_data_improved` pointeur
* Process thread_data_improved.intervals values (intervals.at(i) to intervals.at(i+1)) until every
* intervals value are processed. When there are no more intervals, dump found potential primes into
* thread_data_improved.primes.
* Requires mutex_primes & mutex_intervals initialised.
*
* NOTE: this version a cleaner than the first one `compute_prime_worker`, yet it still has small
* issues. The main one being the load not being equaly shared amoung thread. For example, the input
* contains some small intervals and a giant one, we need to wait until the giant one to be
* processed. 
*/
void * compute_prime_improved_worker(void * data) {
	// Retrieve data from master
	struct thread_data_improved * tdi = (struct thread_data_improved *) data;
	// Local primes found by the worker. To be merge with tdi.primes when the worker is done
	std::vector<mpz_class> worker_primes = {};
	gmp_randclass *rnd = initialize_seed();

	// While the are intervals to process
	while (true) {
		// Take a new interval of values, if no more intervals, stop.
		pthread_mutex_lock(&mutex_intervals);
		if (tdi->intervals->size() < 2) {
			pthread_mutex_unlock(&mutex_intervals);
			break;
		}
		// Take the new interval
		mpz_class from = tdi->intervals->at(0);
		mpz_class to = tdi->intervals->at(1);
		// Remove the interval from the queue
		tdi->intervals->erase(tdi->intervals->begin()); 
		tdi->intervals->erase(tdi->intervals->begin());
		pthread_mutex_unlock(&mutex_intervals);
		
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
* This function relies on `compute_prime_worker` function. For each intervals, launch `nb_threads`
* to find every likely primes.
*/
std::vector<mpz_class>* compute_prime(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	// Init result vector and mutex
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	pthread_mutex_init(&mutex_count, NULL);
	pthread_mutex_init(&mutex_primes, NULL);

	// Declared threads 
	pthread_t ids[nb_threads];
	// For each intervals
	while (intervals->size() > 0) {
		// Create data structure shared by amoung the threads
		struct thread_data td;
		td.rounds = rounds;
		td.count = intervals->at(0);
		td.max = intervals->at(1);
		intervals->erase(intervals->begin());
		intervals->erase(intervals->begin());
		td.primes = primes;
		td.rnd = initialize_seed();

		// Run the threads, each thread will pick a number in the interval when he is ready
		for(int i = 0; i < nb_threads; i++)
			pthread_create(&ids[i], NULL, &compute_prime_worker, &td);

		// Wait for all thread to finish their job
		for(int i = 0; i < nb_threads; i++)
			pthread_join(ids[i], NULL);
	}

    return primes; // property of caller
}

/*
* Find every (likely) primes in the `intervals`, threaded on `nb_threads`.
* intervals : vector of values representing intervals, [lower_bound1, upper_bound1, lower_bound2,
* upper_bound2, ...]. Improved version of `compute_prime` function, as it creates only `nb_threads`
* and not `nb_threads` per interval.
* rounds : number of miller-rabin approximation rounds, the higher the more precision, but the more
* compute time.
* nb_threads : number of parallel threads launched.
*
* return : vector of unordered likely primes found in the intervals. The pointer needs to be deleted
* by the caller. 
*
* This function relies on `compute_prime_improved_worker` function.
*/
std::vector<mpz_class>* compute_prime_improved(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	// Init result vector and mutex
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	pthread_mutex_init(&mutex_intervals, NULL);
	pthread_mutex_init(&mutex_primes, NULL);

	// Declare threads
	pthread_t ids[nb_threads];

	// Create thread data shared amoung every threads
	struct thread_data_improved tdi{};
	tdi.intervals = intervals;
	tdi.rounds = rounds;
	tdi.primes = primes;

	// Launch threads
	for (int i = 0; i < nb_threads; i++)
		pthread_create(&ids[i], NULL, &compute_prime_improved_worker, &tdi);
	
	// Wait for every threads to finish
	for (int i = 0; i < nb_threads; i++)
		pthread_join(ids[i], NULL);
	
	return primes; // property of caller
}

/*
* Find every (likely) primes in the `intervals`.
* intervals : vector of values representing intervals, [lower_bound1, upper_bound1, lower_bound2,
* upper_bound2, ...].
* rounds : number of miller-rabin approximation rounds, the higher the more precision, but the more
* compute time.
*
* return : vector of unordered likely primes found in the intervals. The pointer needs to be deleted
* by the caller.
*/
std::vector<mpz_class>* compute_prime_unthreaded(std::vector<mpz_class> * intervals, int rounds) {
	// Init result vector and random number generator
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	gmp_randclass *rnd = initialize_seed();
	// Loop through every intervals
	while (intervals->size() > 2) {
		// Lower bound
		mpz_class from = intervals->at(0);
		// Upper bound
		mpz_class to = intervals->at(1);
		// Remove them `from` and `to` from the intervals
		intervals->erase(intervals->begin());
		intervals->erase(intervals->begin());
		// Loop Through every values of the interval
		for (mpz_class i = from; mpz_cmp(i.get_mpz_t(), to.get_mpz_t()) < 0; i++) {
			if (prob_prime(i, rounds, rnd)) { // If the target value is likely prime, store it in result vector
				primes->push_back(i);
			}
		}
	}

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
	
		std::vector<mpz_class> * intervals = new std::vector<mpz_class>();
		std::string line;

		// For each line, parse to find substring<space>substring, 
		// then tries to parse each substring into long integers (base10)
		// represented by GMP as a mpz_class type.
		while (getline(file, line)) {
			std::istringstream iss(line);
			std::string val;
			iss >> val;
			intervals->push_back(mpz_class(val));
			iss >> val;
			intervals->push_back(mpz_class(val));
		}
		file.close();

		// Vector of found likely primes in intervals
		std::vector<mpz_class> * primes;
		// Compute time
		Chrono c(true);
		// Launch computation for every intervals
		//primes = compute_prime(intervals, rounds, nb_thread);
		primes = compute_prime_improved(intervals, rounds, nb_thread);
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