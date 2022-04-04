#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_TARGET_OPENCL_VERSION 300

#ifdef __APPLE__
	#include <OpenCL/opencl.h>
#else
	#include <CL/cl.h>
#endif

#define CL_CHECK(error, message)                                                                                                                     \
	if (error != CL_SUCCESS) {                                                                                                                       \
		std::cerr << "OpenCL Error: " << message << " - code: " << error << std::endl;                                                               \
		exit(EXIT_FAILURE);                                                                                                                          \
	}

#ifndef GIF_PLATFORM_ID
	#define GIF_PLATFORM_ID 3
#endif

#ifdef GIF_DEBUG
std::string printMatrix(double* iMatrix, unsigned int iSize) {
	unsigned int lLineSize = (unsigned int)sqrt(iSize);

	std::stringstream oss;

	oss.precision(5);

	for (int i = 0; i < iSize; i++) {
		if (i % lLineSize == 0) {
			oss << "[";
		}
		oss << iMatrix[i];
		if (i % lLineSize != lLineSize - 1) {
			oss << ", ";
		} else {
			oss << "]" << std::endl;
		}
	}
	return oss.str();
}
#endif

int main(int argc, char** argv) {
	unsigned int lSize = 64;
	// Parse input
	if (argc >= 2) {
		try {
			lSize = atoi(argv[1]);
		} catch (std::exception& e) { std::cerr << "Error: parsing argument <" << argv[1] << ">" << std::endl; }
	} else {
		std::cout << "usage: ./main <mat_size>" << std::endl;
		std::cout << "\tmatsize: default 64" << std::endl;
	}

	// -------------------------------------------------- //
	// ------------------- OPENCL INIT ------------------ //
	// -------------------------------------------------- //

	cl_int clStatus; // Return value of openCL calls, if != CL_SUCCESS, something went wrong

	// -------------------- PLATFORMS ------------------- //

	cl_uint lNumPlatforms;
	clStatus = clGetPlatformIDs(0, NULL, &lNumPlatforms);
	CL_CHECK(clStatus, "Getting platform IDs failed");

	// We need at least 1 available platform
	if (lNumPlatforms <= 0) {
		std::cerr << "No platform available" << std::endl;
		exit(EXIT_FAILURE);
	}

	cl_platform_id* lPlatforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * lNumPlatforms);
	if (lPlatforms == NULL) {
		std::cerr << "Memory allocation for platform(s) failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	// Retrieve platforms
	clStatus = clGetPlatformIDs(lNumPlatforms, lPlatforms, NULL);
	CL_CHECK(clStatus, "Impossible d'obtenir les plateformes à l'aide de clGetPlatformIDs");

#ifdef GIF_DEBUG
	// Print found platforms
	std::cout << lNumPlatforms << " found plateform(s)" << std::endl;
	for (unsigned int i = 0; i < lNumPlatforms; i++) {
		char lBuffer[100];
		std::cout << "Plateform " << i << std::endl;

		clStatus = clGetPlatformInfo(lPlatforms[i], CL_PLATFORM_VENDOR, sizeof(lBuffer), lBuffer, NULL);
		std::cout << "\tVendor: " << lBuffer << std::endl;

		clStatus = clGetPlatformInfo(lPlatforms[i], CL_PLATFORM_NAME, sizeof(lBuffer), lBuffer, NULL);
		std::cout << "\tName: " << lBuffer << std::endl;
	}
#endif

	// -------------------- DEVICES -------------------- //

	/**
	 * Note: On here, we only use CL_DEVICE of type GPU, and the one with the id = GIF_PLATFORM_ID.
	 * This lacks versatility, but it works for what we need. In order to change the id, please define
	 * it through the Makefile of this project, to override the current one.
	*/
	cl_uint lNumDevices;
	clStatus = clGetDeviceIDs(lPlatforms[GIF_PLATFORM_ID], CL_DEVICE_TYPE_GPU, 0, NULL, &lNumDevices);
	CL_CHECK(clStatus, "Getting platform IDs failed");

	// We need at least 1 available device
	if (lNumDevices <= 0) {
		std::cerr << "No device available" << std::endl;
		exit(EXIT_FAILURE);
	}

	cl_device_id* lDevices = (cl_device_id*)malloc(sizeof(cl_device_id) * lNumDevices);
	if (lDevices == NULL) {
		std::cerr << "Memory allocation for device(s) failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	clStatus = clGetDeviceIDs(lPlatforms[GIF_PLATFORM_ID], CL_DEVICE_TYPE_GPU, lNumDevices, lDevices, NULL);
	CL_CHECK(clStatus, "Getting device(s) failed");

#ifdef GIF_DEBUG
	// Display selected devices name
	for (unsigned int i = 0; i < lNumDevices; i++) {
		char lDeviceName[100];
		clGetDeviceInfo(lDevices[i], CL_DEVICE_NAME, sizeof(lDeviceName), lDeviceName, NULL);
		std::cout << "Device " << i << " : " << lDeviceName << std::endl;
	}
#endif

	// Context creation
	cl_context lContext = clCreateContext(NULL, lNumDevices, lDevices, NULL, NULL, &clStatus);
	CL_CHECK(clStatus, "Failed to create context");

	cl_command_queue lCommandQueue = clCreateCommandQueueWithProperties(lContext, lDevices[0], 0, &clStatus);
	CL_CHECK(clStatus, "Failed to create command queue");

	// -------------------- DATA -------------------- //

	double* lMRandom = new double[lSize * lSize]; // Input matrix
	double* lMRes	 = new double[lSize * lSize]; // Result matrix, input as identity
	double* lMDot	 = new double[lSize * lSize]; // Dot product matrix, used for verification later
	double* lBuff	 = new double[lSize];		  // Dot product matrix, used for verification later

	for (size_t i = 0; i < lSize * lSize; ++i) {
		lMRandom[i] = rand() / (double)RAND_MAX;
		lMRes[i]	= 0;
		lMDot[i]	= 0;
	}
	for (unsigned int i = 0; i < lSize; i++) {
		lMRes[i * lSize + i] = 1;
		lBuff[i]			 = 0;
	}

#ifdef GIF_DEBUG
	std::cout << printMatrix(lMRandom, lSize * lSize);
	std::cout << std::endl;
#endif

	// -------------------- PROGRAM -------------------- //

	// Read file from disk, convert it to char* for openCL
	std::ifstream in("gauss.cl");
	if (!in.is_open()) {
		std::cerr << "Could not open the file: gauss.cl" << std::endl;
		exit(EXIT_FAILURE);
	}
	std::string lBuffer	  = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	char*		lCLSource = new char[lBuffer.size() + 1];
	std::copy(lBuffer.begin(), lBuffer.end(), lCLSource);
	lCLSource[lBuffer.size()] = '\0';

	cl_program lProgramme = clCreateProgramWithSource(lContext, 1, (const char**)&lCLSource, 0, &clStatus);
	CL_CHECK(clStatus, "Failed to create program from source");

	clStatus = clBuildProgram(lProgramme, 1, lDevices, NULL, NULL, NULL);

	// If there are build errors, print them to the screen
	if (clStatus != CL_SUCCESS) {
		std::cerr << "Failed to build OpenCL program - code: " << clStatus << std::endl;
		cl_build_status clBuildStatus;
		for (unsigned int i = 0; i < 1; i++) {
			clGetProgramBuildInfo(lProgramme, lDevices[i], CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &clBuildStatus, NULL);
			if (clBuildStatus == CL_SUCCESS) {
				continue;
			}

			char*  lBuildLog;
			size_t lBuildLogSize;

			clGetProgramBuildInfo(lProgramme, lDevices[i], CL_PROGRAM_BUILD_LOG, 0, NULL, &lBuildLogSize);

			lBuildLog = (char*)malloc(lBuildLogSize);
			if (lBuildLog == NULL) {
				std::cerr << "Memory allocation for build log failed" << std::endl;
				exit(EXIT_FAILURE);
			}

			clGetProgramBuildInfo(lProgramme, lDevices[i], CL_PROGRAM_BUILD_LOG, lBuildLogSize, lBuildLog, NULL);
			lBuildLog[lBuildLogSize - 1] = '\0';

			std::cout << "Device " << i << " Build Log : " << std::endl << lBuildLog << std::endl;
			free(lBuildLog);
		}
		exit(EXIT_SUCCESS);
	}
	delete[] lCLSource;

	// -------------------- BUFFERS -------------------- //

	// Tableaux pour contenir les données à réduire.
	cl_mem lBufferInput; // Input buffers on device
	cl_mem lBufferInput2;
	cl_mem lBufferOutput; // Output buffer on device

	// Create a buffer containing lMRandom
	lBufferInput = clCreateBuffer(lContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double) * lSize * lSize, lMRandom, &clStatus);
	CL_CHECK(clStatus, "Failed to create input buffer");

	lBufferInput2 = clCreateBuffer(lContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(double) * lSize, lBuff, &clStatus);
	CL_CHECK(clStatus, "Failed to create input buffer");

	// Create a buffer object (lBufferOutput) with enough space to hold the output data
	lBufferOutput = clCreateBuffer(lContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(double) * lSize * lSize, lMRes, &clStatus);
	CL_CHECK(clStatus, "Failed to create output buffer");

	// -------------------- KERNEL -------------------- //

	cl_kernel lKernel;
	// Create kernel, binded to the function invertParallel
	lKernel = clCreateKernel(lProgramme, "invertParallel", &clStatus);
	CL_CHECK(clStatus, "Failed to create kernel");

	// Set kernel args from buffers
	clStatus = clSetKernelArg(lKernel, 0, sizeof(cl_mem), &lBufferInput);
	CL_CHECK(clStatus, "Failed to set kernel argument: lBufferInput");
	clStatus = clSetKernelArg(lKernel, 1, sizeof(cl_mem), &lBufferInput2);
	CL_CHECK(clStatus, "Failed to set kernel argument: lBufferInput2");
	clStatus = clSetKernelArg(lKernel, 2, sizeof(unsigned int), &lSize);
	CL_CHECK(clStatus, "Failed to set kernel argument: lSize");
	clStatus = clSetKernelArg(lKernel, 3, sizeof(cl_mem), &lBufferOutput);
	CL_CHECK(clStatus, "Failed to set kernel argument: lBufferOutput");

	size_t* lGlobalWorkSize = new size_t(lSize);
	// size_t* lLocalWorkSize	= new size_t(32);
	size_t* lLocalWorkSize = NULL;

	// Time benchmarking
	clock_t lStartTime, lStopTime;
	lStartTime = clock();

	// Kernel Enqueueing - Start GPU job
	clStatus = clEnqueueNDRangeKernel(lCommandQueue, lKernel, 1, NULL, lGlobalWorkSize, lLocalWorkSize, 0, NULL, NULL);
	CL_CHECK(clStatus, "Failed to enqueue kernel");

	// Read output buffer when kernel is done
	clStatus = clEnqueueReadBuffer(lCommandQueue, lBufferOutput, CL_TRUE, 0, sizeof(double) * lSize * lSize, lMRes, 0, NULL, NULL);
	CL_CHECK(clStatus, "Failed to read buffer");

	lStopTime = clock();

#ifdef GIF_DEBUG
	std::cout << printMatrix(lMRes, lSize * lSize);
	std::cout << std::endl;
#endif

	// Dot product and sum to compute error rate
	double lSum = 0;
	for (size_t i = 0; i < lSize; i++) {
		for (size_t j = 0; j < lSize; j++) {
			for (size_t k = 0; k < lSize; k++) {
				lMDot[i * lSize + j] += lMRandom[i * lSize + k] * lMRes[k * lSize + j];
			}
			lSum += lMDot[i * lSize + j];
		}
	}

	std::cout << "Size: " << lSize << " x " << lSize << std::endl;
	std::cout << "Error: " << lSum - lSize << std::endl;

	std::cerr << (double)(lStopTime - lStartTime) / CLOCKS_PER_SEC << std::endl;

	clReleaseKernel(lKernel);
	clReleaseProgram(lProgramme);
	clReleaseCommandQueue(lCommandQueue);
	clReleaseMemObject(lBufferInput);
	clReleaseMemObject(lBufferOutput);
	clReleaseContext(lContext);

	free(lDevices);
	free(lPlatforms);

	delete lGlobalWorkSize;
	delete[] lMDot;
	delete[] lMRandom;
	delete[] lMRes;
	delete[] lBuff;
}
