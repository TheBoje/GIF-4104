__kernel void invertParallel(__global double* iMat, __global double* lBuff, unsigned int iSize, __global double* oRes) {
	int lGlobalID = get_global_id(0);

	// For each row of the matrix
	for (int k = 0; k < iSize; k++) {
		// find the greatest pivot for column k
		double lMax;
		int	   lPivotIndex = k;

		for (int i = k; i < iSize; i++) {
			if (i % iSize == lGlobalID && fabs(iMat[i * iSize + k]) > lMax) {
				lMax		= fabs(iMat[i * iSize + k]);
				lPivotIndex = i;
			}
		}
		// Merge results
		lBuff[lPivotIndex] = lMax;
		barrier(CLK_GLOBAL_MEM_FENCE);

		// Reduce max
		lMax		= lBuff[0];
		lPivotIndex = 0;
		for (int i = 0; i < iSize; i++) {
			if (lMax < lBuff[i]) {
				lMax		= lBuff[i];
				lPivotIndex = i;
			}
		}

		// Check if matrix in invertible
		if (iMat[lPivotIndex * iSize + k] == 0) {
			break; // Matrix not invertible
		}

		// Normalisation
		double lVal = iMat[k * iSize + k];
		for (int i = 0; i < iSize; i++) {
			iMat[k * iSize + i] = iMat[k * iSize + i] / lVal;
			oRes[k * iSize + i] = oRes[k * iSize + i] / lVal;
		}

		// For each rows of the worker
		for (int i = 0; i < iSize; i++) {
			if ((i % iSize) == lGlobalID && i != k) {
				double d = iMat[i * iSize + k];
				for (int j = 0; j < iSize; j++) {
					iMat[i * iSize + j] = iMat[i * iSize + j] - (iMat[k * iSize + j] * d);
					oRes[i * iSize + j] = oRes[i * iSize + j] - (oRes[k * iSize + j] * d);
				}
			}
		}
	}
}
