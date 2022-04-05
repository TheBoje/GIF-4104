#include <chrono>
#include <iostream>

#include "Matrix.hpp"

// Inverser la matrice par la méthode de Gauss-Jordan; implantation séquentielle.
void invertSequential(Matrix& iA) {
	// vérifier que la matrice est carrée
	assert(iA.rows() == iA.cols());
	// construire la matrice [A I]
	MatrixConcatCols lAI(iA, MatrixIdentity(iA.rows()));

	// traiter chaque rangée
	for (size_t k = 0; k < iA.rows(); ++k) {
		// trouver l'index p du plus grand pivot de la colonne k en valeur absolue
		// (pour une meilleure stabilité numérique).
		size_t p	= k;
		double lMax = fabs(lAI(k, k));
		for (size_t i = k; i < lAI.rows(); ++i) {
			if (fabs(lAI(i, k)) > lMax) {
				lMax = fabs(lAI(i, k));
				p	 = i;
			}
		}
		// vérifier que la matrice n'est pas singulière
		if (lAI(p, k) == 0)
			throw std::runtime_error("Matrix not invertible");

		// échanger la ligne courante avec celle du pivot
		if (p != k)
			lAI.swapRows(p, k);

		double lValue = lAI(k, k);
		for (size_t j = 0; j < lAI.cols(); ++j) {
			// On divise les éléments de la rangée k
			// par la valeur du pivot.
			// Ainsi, lAI(k,k) deviendra égal à 1.
			lAI(k, j) /= lValue;
		}

		// Pour chaque rangée...
		for (size_t i = 0; i < lAI.rows(); ++i) {
			if (i != k) { // ...différente de k
				// On soustrait la rangée k
				// multipliée par l'élément k de la rangée courante
				double lValue = lAI(i, k);
				lAI.getRowSlice(i) -= lAI.getRowCopy(k) * lValue;
			}
		}
	}

	// On copie la partie droite de la matrice AI ainsi transformée
	// dans la matrice courante (this).
	for (unsigned int i = 0; i < iA.rows(); ++i) {
		iA.getRowSlice(i) = lAI.getDataArray()[std::slice(i * lAI.cols() + iA.cols(), iA.cols(), 1)];
	}
}

void invertParallel(Matrix& iA) {
	// vérifier que la matrice est carrée
	assert(iA.rows() == iA.cols());

	// construire la matrice [A I]
	MatrixConcatCols lAI(iA, MatrixIdentity(iA.rows()));

// traiter chaque rangée
#pragma acc data copy(lAI)
	{
		for (size_t k = 0; k < iA.rows(); ++k) {
			// trouver l'index p du plus grand pivot de la colonne k en valeur absolue
			// (pour une meilleure stabilité numérique).
			size_t p	= k;
			double lMax = fabs(lAI(k, k));
			for (size_t i = k; i < lAI.rows(); ++i) {
				if (fabs(lAI(i, k)) > lMax) {
					lMax = fabs(lAI(i, k));
					p	 = i;
				}
			}

			// vérifier que la matrice n'est pas singulière
			if (lAI(p, k) == 0)
				throw std::runtime_error("Matrix not invertible");

			// échanger la ligne courante avec celle du pivot
			if (p != k)
				lAI.swapRows(p, k);

			double lValue = lAI(k, k);

#pragma acc parallel loop
			for (size_t j = 0; j < lAI.cols(); ++j) {
				// On divise les éléments de la rangée k
				// par la valeur du pivot.
				// Ainsi, lAI(k,k) deviendra égal à 1.
				lAI(k, j) /= lValue;
			}

// Pour chaque rangée...
#pragma acc parallel loop copyin(k)
			for (size_t i = 0; i < lAI.rows(); ++i) {
				if (i != k) { // ...différente de k
					// On soustrait la rangée k
					// multipliée par l'élément k de la rangée courante
					double lValue = lAI(i, k);
					lAI.getRowSlice(i) -= lAI.getRowCopy(k) * lValue;
				}
			}
		}
	}

// On copie la partie droite de la matrice AI ainsi transformée
// dans la matrice courante (this).
#pragma acc parallel	  loop
	 for (unsigned int i = 0; i < iA.rows(); ++i) {
		 iA.getRowSlice(i) = lAI.getDataArray()[std::slice(i * lAI.cols() + iA.cols(), iA.cols(), 1)];
	 }
}

// Multiplier deux matrices.
Matrix multiplyMatrix(const Matrix& iMat1, const Matrix& iMat2) {
	assert(iMat1.cols() == iMat2.rows());

	Matrix lRes(iMat1.rows(), iMat2.cols());

#pragma acc parallel loop
	for (size_t i = 0; i < lRes.rows(); ++i) {
#pragma acc loop
		for (size_t j = 0; j < lRes.cols(); ++j) {
			lRes(i, j) = (iMat1.getRowCopy(i) * iMat2.getColumnCopy(j)).sum();
		}
	}

	return lRes;
}

int main(int argc, char* argv[]) {
	srand((unsigned)time(NULL));

	unsigned int lMatSize;
	if (argc >= 2) {
		lMatSize = atoi(argv[1]);
	} else {
		std::cout << "usage:" << std::endl << " ./main [mat-size]" << std::endl;
		return EXIT_FAILURE;
	}

	Matrix lA(lMatSize, lMatSize);
	Matrix lB(lMatSize, lMatSize);

	lA = MatrixRandom(lMatSize, lMatSize);
	lB = lA;

	auto lTStart = std::chrono::steady_clock::now();

	invertParallel(lB);
	// invertSequential(lB);

	auto lTEnd = std::chrono::steady_clock::now();

	std::chrono::duration<double> elapsed_seconds = lTEnd - lTStart;

	Matrix lDot = multiplyMatrix(lA, lB);

	std::cout << "Matrix size: " << lA.cols() << std::endl;
	std::cout << "Error: " << lDot.getDataArray().sum() - lMatSize << std::endl;

	std::cerr << elapsed_seconds.count() << std::endl;
	return 0;
}
