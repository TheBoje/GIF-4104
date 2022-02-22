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
#include <omp.h>

#include "Chrono.hpp"
#include "miller-rabin-gmp.hpp"

/*
* Encapsulation comparaison operator for pair of mpz_class. Used for std::sort.
*/
bool comp_pair(std::pair<mpz_class, mpz_class> a, std::pair<mpz_class, mpz_class> b) { 
	return a.first < b.first;
}

/*
* Merge pair of mpz_class as intervals, to reduce overlapping and to not check if a number is prime
* multiples times.
*
* result pointer is property of caller
*/
std::vector<std::pair<mpz_class, mpz_class>>* merge_intervals(std::vector<std::pair<mpz_class, mpz_class>>* intervals) {
	// Sort array by first element of pairs
	std::sort(intervals->begin(), intervals->end(), comp_pair);
	std::vector<std::pair<mpz_class, mpz_class>> *merged = new std::vector<std::pair<mpz_class, mpz_class>>();
	
	for (std::pair<mpz_class, mpz_class> pair : *intervals) {
		// if the list of merged intervals is empty or if the current interval does not overlap with
		// the previous interval, append it.
		if (merged->empty() || (merged->back().second < pair.first)) {
			merged->push_back(pair);
		} else {
			// there is overlap, so we merge the current and previous intervals.
			merged->back().second = std::max(merged->back().second, pair.second);
		}
	}
	return merged; // Property of caller
}

// Define operator< for mpz_class. As gmp is a C library, operators are only defined by functions.
// This is made for convinence of use
bool operator< (const mpz_class& lhs, const mpz_class& rhs){ return mpz_cmp(lhs.get_mpz_t(), rhs.get_mpz_t()) < 0; }

/* Find every likely prime value in the intervals, multi-threaded using openMP
 * 
 * rounds : number of passes of miller rabin algorithm. Higher means more precision and compute
 * time.
 * nb_threads : number of threads launched to compute. If openMP is not available, defaults as 1 thread.
 * 
 * result : vector of found likely primes in intervals. Property of caller.
*/
std::vector<mpz_class>* compute_prime(std::vector<std::pair<mpz_class, mpz_class>> * intervals, int rounds, int nb_threads) {
	// Init result array
	std::vector<mpz_class>* primes = new std::vector<mpz_class>();
	omp_set_num_threads(nb_threads);
	// Start parallel for loop
	#pragma omp parallel for shared(primes, intervals, rounds)
	for (int i = 0; i < intervals->size(); i++) {
		// Store found primes in a local array to reduce conflicts
		std::vector<mpz_class> local_primes{};
		std::pair<mpz_class, mpz_class> pair = intervals->at(i);
		gmp_randclass* rnd = initialize_seed();
		// Iterates through every item of the interval
		/* Note: for maximum performances, it would be wise to use openMP for this loop too but
		 * because of its implementation, the for loop must use int as iterator, which imply some
		 * refactor for this. I'm not sure if an int would be enough to not overflow, with merged
		 * intervals too. For now, this is giving good performances. If more is needed, this can be
		 * a good place to look into.
		 * Note: it seems to be possible to iterate over an std::vector<T> with openMP parallelism,
		 * it would be the way to improve this.
		*/
		for (mpz_class item = pair.first; item < pair.second; item++) {
			if (prob_prime(item, rounds, rnd)) { // Add found prime in the local array
				local_primes.push_back(item);
			}
		}
		// When the interval is done, add found primes to the shared vector
		primes->insert(primes->end(), local_primes.begin(), local_primes.end());
	}
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
		std::vector<std::pair<mpz_class, mpz_class>> * merged = merge_intervals(intervals);
		Chrono c(true);
		// Launch computation for every intervals
		primes = compute_prime(merged, rounds, nb_thread);
		c.pause();
		// Print every found likely primes in order
		std::sort(primes->begin(), primes->end());
		for (mpz_class p : *primes) {
			std::cout << p << std::endl;
		}
		
		// Time to compute
		std::cerr << c.get() << std::endl;

		// Clean allocations
		delete(intervals);
		delete(merged);
		delete(primes);
		
	} else {
		std::cerr << "error: can\'t open file at : " << argv[2] << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}