#include "Matrix.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mpi.h>
#include <stdexcept>
#include <unistd.h>

struct lDataPivot {
	int	   index;
	double val;
};

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

// Inverser la matrice par la méthode de Gauss-Jordan; implantation MPI parallèle.
void invertParallel(Matrix& iA) {
	// Is the matrix square
	assert(iA.rows() == iA.cols());
	// Construct [A I] matrix
	MatrixConcatCols lAI(iA, MatrixIdentity(iA.rows()));

	unsigned int lRank = MPI::COMM_WORLD.Get_rank();
	unsigned int lSize = MPI::COMM_WORLD.Get_size();

	// For each column of the matrix
	for (size_t k = 0; k < lAI.rows(); k++) {
		// Find greatest (abs) pivot for column k
		double lMax;
		size_t lPivotIndex = k;
		for (size_t i = k; i < lAI.rows(); i++) {
			if ((i % lSize) == lRank && fabs(lAI(i, k)) > lMax) {
				lMax		= fabs(lAI(i, k));
				lPivotIndex = i;
			}
		}

		// Data per worker, containing pivot value found above
		lDataPivot lIn, lOut;
		lIn.index = lPivotIndex;
		lIn.val	  = lMax;
		// Find who has the greatest pivot value between every workers
		MPI::COMM_WORLD.Allreduce(&lIn, &lOut, lSize, MPI_DOUBLE_INT, MPI_MAXLOC);
		lPivotIndex = lOut.index;

		MPI::COMM_WORLD.Bcast(&lAI(lPivotIndex, 0), lAI.cols(), MPI::DOUBLE, lPivotIndex % lSize);

		// Check if matrix is not a singular matrix
		if (lAI(lPivotIndex, k) == 0)
			throw std::runtime_error("Matrix not invertible");

		// Swap lPivotIndex and k rows
		if (lPivotIndex != k)
			lAI.swapRows(lPivotIndex, k);

		// Normalization
		double lValue = lAI(k, k);
		for (size_t j = 0; j < lAI.cols(); j++) {
			lAI(k, j) /= lValue;
		}

		// For each rows
		for (size_t i = 0; i < lAI.rows(); i++) {
			if ((i % lSize) == lRank && i != k) {
				lAI.getRowSlice(i) -= lAI.getRowCopy(k) * lAI(i, k);
			}
		}

		for (size_t i = 0; i < lAI.rows(); i++) {
			MPI::COMM_WORLD.Bcast(&lAI(i, 0), lAI.cols(), MPI::DOUBLE, i % lSize);
		}
	}

	// Copy right side of the (n * 2n) matrix into (n * n).
	for (unsigned int i = 0; i < iA.rows(); i++) {
		iA.getRowSlice(i) = lAI.getDataArray()[std::slice(i * lAI.cols() + iA.cols(), iA.cols(), 1)];
	}
}

// Multiplier deux matrices.
Matrix multiplyMatrix(const Matrix& iMat1, const Matrix& iMat2) {
	// vérifier la compatibilité des matrices
	assert(iMat1.cols() == iMat2.rows());
	// effectuer le produit matriciel
	Matrix lRes(iMat1.rows(), iMat2.cols());
	// traiter chaque rangée
	for (size_t i = 0; i < lRes.rows(); ++i) {
		// traiter chaque colonne
		for (size_t j = 0; j < lRes.cols(); ++j) {
			lRes(i, j) = (iMat1.getRowCopy(i) * iMat2.getColumnCopy(j)).sum();
		}
	}
	return lRes;
}

int main(int argc, char** argv) {
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

	MPI::Init();
	int lRank = MPI::COMM_WORLD.Get_rank();

	if (lRank == 0) {
		lA = MatrixRandom(lMatSize, lMatSize);
		lB = lA;
	}

	MPI::COMM_WORLD.Bcast(&lB(0, 0), lMatSize * lMatSize, MPI::DOUBLE, 0);

	double lTStart, lTEnd;
	lTStart = MPI_Wtime();
	// invertParallel(lB);
	invertSequential(lB);
	lTEnd = MPI_Wtime();

	if (lRank == 0) {
		Matrix lDot = multiplyMatrix(lA, lB);
		std::cout << "Matrix size: " << lA.cols() << std::endl;
		std::cout << "Error: " << lDot.getDataArray().sum() - lMatSize << std::endl;
		std::cerr << lTEnd - lTStart << std::endl;
	}

	MPI::Finalize();
	return 0;
}
