#include <stdlib.h>
#include "genetic_algorithm.h"
#include <pthread.h>

int main(int argc, char *argv[]) {
	// array with all the objects that can be placed in the sack
	sack_object *objects = NULL;

	// number of objects
	int object_count = 0;

	// maximum weight that can be carried in the sack
	int sack_capacity = 0;

	// number of generations
	int generations_count = 0;

	// number of threads
	int thread_number = 0;

	if (!read_input(&objects, &object_count, &sack_capacity, &generations_count, &thread_number, argc, argv)) {
		return 0;
	}

	// current generation
	individual *current = (individual *)calloc(object_count, sizeof(individual));

	// next generation
	individual *next = (individual *)calloc(object_count, sizeof(individual));
	
	// init and create barrier
	pthread_barrier_t barrier;
	pthread_barrier_init(&barrier, NULL, thread_number);

	pthread_t threads[thread_number];
	ThreadData threadData[thread_number];
	void *status;

	// creating  threads
	for (int i = 0; i < thread_number; i++) {

		// initializing thread arguments
		threadData[i].id = i;
		threadData[i].object_count = object_count;
		threadData[i].generations_count = generations_count;
		threadData[i].sack_capacity = sack_capacity;
		threadData[i].thread_number = thread_number;
		threadData[i].objects = objects;
		threadData[i].current_generation = &current;
		threadData[i].next_generation = &next;
		threadData[i].barrier = &barrier;
		pthread_create(&threads[i], NULL, parallel, &threadData[i]);
	}

	// waiting fot threads to finish
	for (int i = 0; i < thread_number; i++) {
		pthread_join(threads[i], &status);
	}

	// free
	free(objects);
	pthread_barrier_destroy(&barrier);

	return 0;
}
