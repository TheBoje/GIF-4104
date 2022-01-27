#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <pthread.h>
#include <semaphore.h>
#include <gmpxx.h>

#include "Chrono.hpp"
#include "miller-rabin-gmp.hpp"

struct thread_data {
  	int round;
  	mpz_class count;
  	mpz_class max;
	std::vector<mpz_class> * primes;
};

pthread_mutex_t mutex_vec;
pthread_mutex_t mutex_count;

void * compute_prime(void * data) {
  	struct thread_data * td = (struct thread_data *)data;
	// std::cout << pthread_self() << " " << td << " " << td->primes->size() << std::endl;
	gmp_randclass *rnd = initialize_seed();
	for (;;) {

		// Get the actual count and verify if it is not above the up part of the interval
		pthread_mutex_lock(&mutex_count);
		//  std::cout << "count:" << td->count << " max:" << td->max << " thread:" << pthread_self() << std::endl;
		if (td->count >= td->max) {pthread_mutex_unlock(&mutex_count); break;}
		mpz_class target = td->count;
		td->count++;
		pthread_mutex_unlock(&mutex_count);
		// Run the Miller-Rabin algorithm  
		bool res = prob_prime(target, td->round, rnd);

		// if the number is considered as prime, write it in the result vector
		if (res) {
			pthread_mutex_lock(&mutex_vec);
			td->primes->push_back(target);
			pthread_mutex_unlock(&mutex_vec);
		}
	}
	delete(rnd);
  	pthread_exit(EXIT_SUCCESS);
}

void compute_prime_interval(mpz_class a, mpz_class b, int round, int nb_threads) {
	
	std::vector<mpz_class> * primes = new std::vector<mpz_class>;
	primes->reserve(400000);

	// Declared threads and  Init mutex
	pthread_t ids[nb_threads];
	
	pthread_mutex_init(&mutex_count, NULL);
	pthread_mutex_init(&mutex_vec, NULL);

	// Create data structure shared by all the threads
	struct thread_data td;
	td.round = round;
	td.count = a;
	td.max = b;
	td.primes = primes;
	
	Chrono c(true);

	// Run the threads, each thread will pick a number in the interval when he is ready
	std::cout << "starting threads" << std::endl;
	for(int i = 0; i < nb_threads; i++)
		pthread_create(&ids[i], NULL, &compute_prime, &td);

	for(int i = 0; i < nb_threads; i++) {
		pthread_join(ids[i], NULL);
    }


	c.pause();
	std::sort(primes->begin(), primes->end());

	for (mpz_class val : *primes) {
		// std::cout << val << std::endl;
	}
	
	std::cerr << c.get() << " sec, count:" << primes->size() << std::endl;
    delete(primes);
}

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cerr << "usage: executable <nb_threads> <filepath> [rounds]" << std::endl; 
		// return EXIT_FAILURE;
	}
	int rounds = 10;
	if (argc >= 4) {
		// rounds = atoi(argv[3]);
	}

	// int nb_thread = atoi(argv[1]);
	int nb_thread = 8;


	std::ifstream file;
	// file.open(argv[2]);
	file.open("../tests/1_simple.txt");
	if (file.is_open()) {
		// std::string line, s_a, s_b;
		// mpz_t a, b; 
		// while (getline(file, line)) {

		// mpz_set_str(a, s_a.c_str(), 10);
		// mpz_set_str(b, s_b.c_str(), 10);
		// std::cout << s_b << std::endl;
		compute_prime_interval(mpz_class(400), mpz_class(40000000), rounds, nb_thread);
		// }
		file.close();
	} else {
		std::cerr << "error: can\'t open file at : " << argv[2] << std::endl;
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}