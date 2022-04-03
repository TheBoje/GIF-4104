#include <iostream>
#include <chrono>

#include "Matrix.hpp"

#define N 1000

// Multiplier deux matrices.
Matrix multiplyMatrix(const Matrix& iMat1, const Matrix& iMat2) {
	assert(iMat1.cols() == iMat2.rows());

	Matrix lRes(iMat1.rows(), iMat2.cols());
    
    // #pragma acc parallel
    {
        // #pragma acc loop
        for (size_t i = 0; i < lRes.rows(); ++i) {
            // #pragma acc loop
            for (size_t j = 0; j < lRes.cols(); ++j) {
                lRes(i, j) = (iMat1.getRowCopy(i) * iMat2.getColumnCopy(j)).sum();
            }
        }
    }

	return lRes;
}

int main(int argc, char * argv[]) {

    Matrix mat1(N, N);
    Matrix mat2(N, N);

    mat1 = MatrixRandom(N, N);
    mat2 = MatrixRandom(N, N);

    auto start = std::chrono::steady_clock::now();

    Matrix res = multiplyMatrix(mat1, mat2);

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

    // std::cout << "res = \n" << res.str() << std::endl;
}
