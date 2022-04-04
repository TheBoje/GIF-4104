#include <iostream>
#include <chrono>

#include "Matrix.hpp"

#define N 1000

void invertSequential(Matrix& iA) {
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
            #pragma acc parallel loop copyin(k)
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
    // #pragma acc parallel loop
	for (unsigned int i = 0; i < iA.rows(); ++i) {
		iA.getRowSlice(i) = lAI.getDataArray()[std::slice(i * lAI.cols() + iA.cols(), iA.cols(), 1)];
	}
}


// Multiplier deux matrices.
Matrix multiplyMatrix(const Matrix& iMat1, const Matrix& iMat2) {
	assert(iMat1.cols() == iMat2.rows());

	Matrix lRes(iMat1.rows(), iMat2.cols());
    
    #pragma acc parallel
    {
        #pragma acc loop
        for (size_t i = 0; i < lRes.rows(); ++i) {
            #pragma acc loop
            for (size_t j = 0; j < lRes.cols(); ++j) {
                lRes(i, j) = (iMat1.getRowCopy(i) * iMat2.getColumnCopy(j)).sum();
            }
        }
    }

	return lRes;
}

int main(int argc, char * argv[]) {



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

	double lTStart, lTEnd;

    lA = MatrixRandom(lMatSize, lMatSize);
    lB = lA;

	// invertParallel(lB);

    auto start = std::chrono::steady_clock::now();
	invertSequential(lB);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;

    auto start_mult = std::chrono::steady_clock::now();
    Matrix lDot = multiplyMatrix(lA, lB);
    auto end_mult = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds_mult = end_mult - start_mult;

    std::cout << "Matrix size: " << lA.cols() << std::endl;
    std::cout << "Error: " << lDot.getDataArray().sum() - lMatSize << std::endl;
    // std::cout << lA.str() << std::endl << std::endl;
    // std::cout << lB.str() << std::endl << std::endl;
    // std::cout << multiplyMatrix(lA, lB).str() << std::endl << std::endl;

    std::cout << "elapsed time: " << elapsed_seconds.count() << "s and elapsed time multiplication: " << elapsed_seconds_mult.count() << "s\n";

	return 0;
}
