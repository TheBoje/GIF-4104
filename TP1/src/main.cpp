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
* Data shared from `compute_prime_1` to each `compute_prime_1_worker` thread.
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
struct thread_data_1 {
	std::vector<mpz_class> * primes;
	gmp_randclass *rnd;
  	int rounds;
  	mpz_class count;
  	mpz_class max;
};


/*
* Data shared from `compute_prime_2` to each `compute_prime_2_worker` thread.
* intervals : array of intervals. Shared amoung every worker. When a thread is free, it read the
* next 2 values from `index` (as lower and upper bounds of an interval) and can compute primes from
* them. 
* primes : return vector of values. Shared amoung every worker, it need to be accessed (read &
* write) with a mutex (`mutex_primes`). When the stack of job is empty, before stopping the worker
* will take the mutex and write his found primes into `primes`. Initialised as empty.
* rounds : number of rounds of miller-rabin algorithm to do. The higher the more accurate the result
* is, but the more expensive (time) it is.
* index : index of the next available interval. Initialised at 0, worker need the mutex
* `mutex_index` to read OR write. `-1` value means not more intervals.
*/
struct thread_data_2 {
	std::vector<mpz_class> * intervals;
	std::vector<mpz_class> * primes;
	int rounds;
	int index;
};

// To read/write thread_data_1.primes or thread_data_2.primes
pthread_mutex_t mutex_primes;
// To read/write thread_data_1.count
pthread_mutex_t mutex_count;
// To read/write thread_data_1.intervals
pthread_mutex_t mutex_intervals;
// To read/write thread_data_2.index
pthread_mutex_t mutex_index;

/*
* Thread function to find every primes between two mpz_class values.
* data : `thread_data_1` pointer
* Process thread_data_1.count to thread_data_1.max values, push each potential prime value into
* thread_data_1.prime until there is no more values to test. 
* Requires mutex_primes & mutex_count initialised.
*/
void * compute_prime_1_worker(void * data) {
	// Retrieve data provided from master
  	struct thread_data_1 * td = (struct thread_data_1 *)data;
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

		// if the number is (likely) prime, write it in the local vector
		if (res) {
			worker_primes.push_back(target);
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
* data : `thread_data_2` pointeur
* Process thread_data_2.intervals values (intervals.at(index) to intervals.at(index+1)) until every
* intervals value are processed. When there are no more intervals, dump found potential primes into
* thread_data_2.primes.
* Requires mutex_primes & mutex_index initialised.
*/
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
* This function relies on `compute_prime_1_worker` function. For each intervals, launch `nb_threads`
* to find every likely primes.
*/
std::vector<mpz_class>* compute_prime_1(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	// Init result vector and mutex
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	pthread_mutex_init(&mutex_count, NULL);
	pthread_mutex_init(&mutex_primes, NULL);

	// Declared threads 
	pthread_t ids[nb_threads];
	// For each intervals
	for (int j = 0; j < intervals->size(); j+=2) {
		// Create data structure shared by amoung the threads
		struct thread_data_1 td;
		td.rounds = rounds;
		td.count = intervals->at(j);
		td.max = intervals->at(j+1);
		td.primes = primes;
		td.rnd = initialize_seed();

		// Run the threads, each thread will pick a number in the interval when he is ready
		/*
		* NOTE: Creating nb_threads for each interval is expensive, mostly when there is a lot of
		* small intervals. If it is the type of input data you have, maybe consider using
		* `compute_prime_2` which provides better performances for this case (but worst for few big
		* sized intervals). 
		*/
		for(int i = 0; i < nb_threads; i++)
			pthread_create(&ids[i], NULL, &compute_prime_1_worker, &td);

		// Wait for all thread to finish their job
		for(int i = 0; i < nb_threads; i++)
			pthread_join(ids[i], NULL);
	}

    return primes; // property of caller
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
std::vector<mpz_class>* compute_prime_2(std::vector<mpz_class> * intervals, int rounds, int nb_threads) {
	// Init result vector and mutex
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	pthread_mutex_init(&mutex_index, NULL);
	pthread_mutex_init(&mutex_primes, NULL);

	// Declare threads
	pthread_t ids[nb_threads];

	// Create thread data shared amoung every threads
	struct thread_data_2 tdi{};
	tdi.intervals = intervals;
	tdi.rounds = rounds;
	tdi.primes = primes;
	tdi.index = 0;

	// Launch threads
	for (int i = 0; i < nb_threads; i++)
		pthread_create(&ids[i], NULL, &compute_prime_2_worker, &tdi);
	
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
	for (int i = 0; i < intervals->size(); i+=2) {
		// Lower bound
		mpz_class from = intervals->at(i);
		// Upper bound
		mpz_class to = intervals->at(i+1);
		// Remove them `from` and `to` from the intervals
		// Loop Through every values of the interval
		for (mpz_class j = from; mpz_cmp(j.get_mpz_t(), to.get_mpz_t()) < 0; j++) {
			if (prob_prime(j, rounds, rnd)) { // If the target value is likely prime, store it in result vector
				primes->push_back(j);
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
		// primes = compute_prime_unthreaded(intervals, rounds);
		// primes = compute_prime_1(intervals, rounds, nb_thread);
		primes = compute_prime_2(intervals, rounds, nb_thread);
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