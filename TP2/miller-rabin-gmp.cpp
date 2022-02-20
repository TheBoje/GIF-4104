/*
 * The Miller-Rabin primality tester with GNU MP (GMP) big numbers.
 *
 * Copyright 2017 Christian Stigen Larsen
 * Distributed under the modified BSD license.
 */

#include "miller-rabin-gmp.hpp"

/*
 * Calculates a^x mod n through modular exponentiation.
 *
 * This algorithm is taken from Bruce Schneier's book "Applied Cryptography".
 * See http://en.wikipedia.org/wiki/Modular_exponentiation
 */
mpz_class pow_mod(mpz_class a, mpz_class x, const mpz_class& n)
{
	mpz_class r = 1;
	mpz_powm(r.get_mpz_t(), a.get_mpz_t(), x.get_mpz_t(), n.get_mpz_t());
	return r;
}


/*
 * Seeds the random number generator with random bytes from /dev/urandom or, if
 * bytes is zero, from the current time.
 */
gmp_randclass * initialize_seed()
{
	gmp_randclass *res = new gmp_randclass(gmp_randinit_default);
	// Seed with the current time
	res->seed(time(NULL));
	return res;
}

/*
 * Returns a uniform random integer in the inclusive range.
 */
mpz_class randint(const mpz_class& lowest, const mpz_class& highest, gmp_randclass *rnd)
{
	if ( lowest == highest )
		return lowest;

	return rnd->get_z_range(highest - lowest + 1) + lowest;
}

/*
 * Ah, yes. The Miller-Rabin probabilistic prime testing algorithm. A fine
 * piece of work, it is.
 *
 * This implementation does not like negative numbers. Deal with it.
 */
static bool miller_rabin_backend(const mpz_class& n, const size_t rounds, gmp_randclass *rnd)
{
	// Treat n==1, 2, 3 as a primes
	if (n == 1 || n == 2 || n == 3)
		return true;

	// Treat negative numbers in the frontend
	if (n <= 0)
		return false;

	// Even numbers larger than two cannot be prime
	if ((n & 1) == 0)
		return false;

	// Write n-1 as d*2^s by factoring powers of 2 from n-1
	size_t s = 0;
	{
		mpz_class m = n - 1;
		while ((m & 1) == 0) {
			++s;
			m >>= 1;
		}
	}
	const mpz_class d = (n - 1) / (mpz_class(1) << s);

	for (size_t i = 0; i < rounds; ++i) {
		const mpz_class a = randint(2, n - 2, rnd);
		mpz_class x = pow_mod(a, d, n);

		if (x ==1 || x == (n - 1))
			continue;

		for (size_t r = 0; r < (s-1); ++r) {
			x = pow_mod(x, 2, n);
			if (x == 1) {
				// Definitely not a prime
				return false;
			}
			if (x == n - 1)
				break;
		}

		if (x != (n - 1)) {
			// Definitely not a prime
			return false;
		}
	}

	// Might be prime
	return true;
}

/*
 * The Miller-Rabin front end.
 */
bool prob_prime(const mpz_class& n, const size_t rounds, gmp_randclass *rnd) { 
	return miller_rabin_backend(n > 0 ? n : -n, rounds, rnd); 
}