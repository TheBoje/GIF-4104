#include <iostream>
#include <chrono>

#define N 1000000000

int main(int argc, char * argv[]) {

    int *a = (int *)malloc(sizeof(int) * N);

    auto start = std::chrono::steady_clock::now();

    #pragma acc parallel loop copyout(a)
    for(int i = 0; i < N; i++) 
        a[i] = 2*i;

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
}
