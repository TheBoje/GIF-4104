#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

void * threadHelloWorld(void * arg) {
    printf("Hello, World!\n");
    return EXIT_SUCCESS;
}

int main(int argc, char** argv) {
    pthread_t thread;
    int res = pthread_create(&thread, NULL, threadHelloWorld, NULL);
    if (!res) {
        pthread_join(thread, NULL);
        return EXIT_SUCCESS;
    }    
    fprintf("%s\n", strerror(res), stderr);
    return EXIT_FAILURE;
}