#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "genetic_algorithm.h"
#include "pthread.h"

// added thread_number parameter.
int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int *thread_number, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 4) {
		fprintf(stderr, "Usage:\n\t./tema1 in_file generations_count\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	*thread_number = (int) strtol(argv[3], NULL, 10);

	if (*thread_number == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;
	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

int cmpfunc(const void *a, const void *b)
{
	int i;
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {
		int first_count = 0, second_count = 0;

		for (i = 0; i < first->chromosome_length && i < second->chromosome_length; ++i) {
			first_count += first->chromosomes[i];
			second_count += second->chromosomes[i];
		}

		res = first_count - second_count; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

/**
 * Function which computes the fitness for every individual.
 * Function is taken from the homework skel and parallelized.
 * I parallelized the outer for loop according to start and end,
 * which were calculated starting from the thread id. 
 **/ 
void compute_fitness_function(const sack_object *objects, individual *generation, int sack_capacity, int start, int end)
{
	int weight;
	int profit;

	for (int i = start; i < end; ++i) {
		weight = 0;
		profit = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				weight += objects[j].weight;
				profit += objects[j].profit;
			}
		}

		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

/**
 * Function which merges 2 sorted arrays in decreasing order.
 * Function receives the current_generation, the indexes of descending part(current generation is divided in 
 * "thread_number" descending parts), merged array computed so far, its length and object_count.
 * returns an array of individuals which represents the sorted result. 
 **/ 
individual* merge(individual *source, int start, int end, individual *dest, int dest_size, int object_count) {
	int aux_length;
	individual *aux;

	aux_length = dest_size + (end - start);
	aux = (individual*)malloc(aux_length * sizeof(individual));

	int i, j, k;
	i = start;
	j = 0;
	k = 0;
	while (i < end && j < dest_size) {
		if (source[i].fitness >= dest[j].fitness) {
			aux[k] = source[i];
			k++;
			i++;
		} else {
			aux[k] = dest[j];
			k++;
			j++;
		}
	}

	while (i < end) {
		aux[k] = source[i];
		k++;
		i++;
	}

	while (j < dest_size) {
		aux[k] = dest[j];
		k++;
		j++;
	}

	if (dest != NULL) {
		free(dest);
	}
	
	return aux;
}

int min(int a, int b)
{
	return (a < b ? a : b);
}

void *parallel(void *arg) {
	ThreadData thread = *((ThreadData *) arg);
	int count, cursor;
	int start, end;
	individual *tmp = NULL;

	start = thread.id * (double)thread.object_count / thread.thread_number;
	end = min((thread.id + 1) * (double)thread.object_count / thread.thread_number, thread.object_count);

	// parallelization of current and next generation.
	for (int i = start; i < end; ++i) {
		(*thread.current_generation)[i].fitness = 0;
		(*thread.current_generation)[i].chromosomes = (int*) calloc(thread.object_count, sizeof(int));
		(*thread.current_generation)[i].chromosomes[i] = 1;
		(*thread.current_generation)[i].index = i;
		(*thread.current_generation)[i].chromosome_length = thread.object_count;

		(*thread.next_generation)[i].fitness = 0;
		(*thread.next_generation)[i].chromosomes = (int*) calloc(thread.object_count, sizeof(int));
		(*thread.next_generation)[i].index = i;
		(*thread.next_generation)[i].chromosome_length = thread.object_count;
	}

	// iterate over each generation
	for (int k = 0; k < thread.generations_count; ++k) {
		cursor = 0;

		// compute fitness for each generation.
		start = thread.id * (double)thread.object_count / thread.thread_number;
		end = min((thread.id + 1) * (double)thread.object_count / thread.thread_number, thread.object_count);
		compute_fitness_function(thread.objects, (*thread.current_generation), thread.sack_capacity, start, end);
		pthread_barrier_wait(thread.barrier);

		// sort current generation in thread_number decreasing sorted parts.
		qsort((*thread.current_generation) + start, end - start, sizeof(individual), cmpfunc);
		pthread_barrier_wait(thread.barrier);


		// Merge all thread_number decreasing parts using merge function on only one thread. 
		if (thread.id == 0) {
			int aux_length = 0;
			individual *aux = NULL;

			for (int i = 0; i < thread.thread_number; i++) {
				int start_merge = i * (double)thread.object_count / thread.thread_number;
				int end_merge = min((i + 1) * (double)thread.object_count / thread.thread_number, thread.object_count);
				aux = merge(*thread.current_generation, start_merge, end_merge, aux, aux_length, thread.object_count);
				aux_length += end_merge - start_merge;
			}

			// copy the merge back to current generation
			for (int j = 0; j < aux_length; j++) {
					(*thread.current_generation)[j] = aux[j];
			}
			free(aux);
		}
		pthread_barrier_wait(thread.barrier);

		// keep first 30% children (elite children selection)
		// split in thread_number parts.
		count = thread.object_count * 3 / 10;
		start = thread.id * (double)count / thread.thread_number;
		end = min((thread.id + 1) * (double)count / thread.thread_number, count);
		for (int i = start; i < end; ++i) {
				copy_individual((*thread.current_generation) + i, (*thread.next_generation) + i);
		}
		cursor = count;

		// mutate first 20% children with the first version of bit string mutation
		// split in thread_number parts.
		count = thread.object_count * 2 / 10;
		start = thread.id * (double)count / thread.thread_number;
		end = min((thread.id + 1) * (double)count / thread.thread_number, count);
		for (int i = start; i < end; ++i) {
			copy_individual((*thread.current_generation) + i, (*thread.next_generation) + cursor + i);
			mutate_bit_string_1((*thread.next_generation) + cursor + i, k);
		}
		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		// split in thread_number parts.
		count = thread.object_count * 2 / 10;
		start = thread.id * (double)count / thread.thread_number;
		end = min((thread.id + 1) * (double)count / thread.thread_number, count);
		for (int i = start; i < end; ++i) {
			copy_individual((*thread.current_generation) + i + count, (*thread.next_generation) + cursor + i);
			mutate_bit_string_2((*thread.next_generation) + cursor + i, k);
		}
		cursor += count;
		pthread_barrier_wait(thread.barrier);

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		// crossover on only one thread.
		count = thread.object_count * 3 / 10;
		if (thread.id == 0) {
			if (count % 2 == 1) {
				copy_individual((*thread.current_generation) + thread.object_count - 1,
					(*thread.next_generation) + cursor + count - 1);
				count--;
			}

			for (int i = 0; i < count; i += 2) {
				crossover((*thread.current_generation) + i, (*thread.next_generation) + cursor + i, k);
			}
		}	
		pthread_barrier_wait(thread.barrier);

		// switch to new generation
		// switch generation on only one thread.
		if (thread.id == 0) {
			tmp = (*thread.current_generation);
			(*thread.current_generation) = (*thread.next_generation);
			(*thread.next_generation) = tmp;
		}
		pthread_barrier_wait(thread.barrier);

		// reindexing
		// split in thread_number parts.
		start = thread.id * (double)thread.object_count / thread.thread_number;
		end = min((thread.id + 1) * (double)thread.object_count / thread.thread_number,
			thread.object_count);
		for (int i = start; i < end; ++i) {
			(*thread.current_generation)[i].index = i;
		}
		pthread_barrier_wait(thread.barrier);

		// print best fitness on only one thread.
		if (k % 5 == 0 && thread.id == 0) {
			print_best_fitness((*thread.current_generation));
		}
		pthread_barrier_wait(thread.barrier);
		
	}

	pthread_barrier_wait(thread.barrier);
	start = thread.id * (double)thread.object_count / thread.thread_number;
	end = min((thread.id + 1) * (double)thread.object_count / thread.thread_number, thread.object_count);
	compute_fitness_function(thread.objects, (*thread.current_generation), thread.sack_capacity, start, end);
	pthread_barrier_wait(thread.barrier);

	qsort((*thread.current_generation) + start, end - start, sizeof(individual), cmpfunc);
	pthread_barrier_wait(thread.barrier);

	if (thread.id == 0) {
		int aux_length = 0;
		individual *aux = NULL;

		for (int i = 0; i < thread.thread_number; i++) {
			int start_merge = i * (double)thread.object_count / thread.thread_number;
			int end_merge = min((i + 1) * (double)thread.object_count / thread.thread_number, thread.object_count);
			aux = merge(*thread.current_generation, start_merge, end_merge, aux, aux_length, thread.object_count);
			aux_length += end_merge - start_merge;
		}
		for (int j = 0; j < aux_length; j++) {
				(*thread.current_generation)[j] = aux[j];
		}
		free(aux);
	}
	pthread_barrier_wait(thread.barrier);

	if (thread.id == 0) {
		print_best_fitness(*thread.current_generation);

		// free resources for old generation
		free_generation(*thread.current_generation);
		free_generation(*thread.next_generation);

		// free resources
		free(*thread.current_generation);
		free(*thread.next_generation);
	}

	pthread_exit(NULL);
}
