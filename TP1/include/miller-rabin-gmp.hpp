#ifndef MILLER_RABIN_GMP_H
#define MILLER_RABIN_GMP_H

/*
 * The Miller-Rabin primality tester with GNU MP (GMP) big numbers.
 *
 * Copyright 2017 Christian Stigen Larsen
 * Distributed under the modified BSD license.
 */

#include <gmpxx.h>
#include <stdint.h>

bool prob_prime(const mpz_class& n, const size_t rounds, gmp_randclass *rnd);
extern "C" gmp_randclass * initialize_seed();
mpz_class pow_mod(mpz_class a, mpz_class x, const mpz_class& n);
mpz_class randint(const mpz_class& lowest, const mpz_class& highest, gmp_randclass * rnd);

#endif //! MILLER_RABIN_GMP_H