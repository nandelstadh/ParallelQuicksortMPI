#include "quicksort.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pivot.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Expected: quicksort input output pivot\n");
    }
    char* input = argv[0];
    char* output = argv[1];
    int pivot_strategy = atoi(argv[2]);
    int** elements;
    int** my_elements;
    int n, local_n, myid, n_proc;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    n = read_input(input, elements);
    local_n = distribute_from_root(*elements, n, my_elements);
    qsort(my_elements, local_n, sizeof(int), compare);

    double start = MPI_Wtime();
    global_sort(elements, n, MPI_COMM_WORLD, pivot_strategy);
    double time = MPI_Wtime() - start;
    printf("Time elapsed: %f\n", time);

    gather_on_root(*elements, *my_elements, local_n);
    check_and_print(*elements, n, output);

    return 0;
}

/**
 * Distribute all elements from root to the other processes as evenly as
 * possible. Note that this method allocates memory for my_elements. This must
 * be freed by the caller!
 * @param all_elements Elements to distribute (Not significant in other processes)
 * @param n Number of elements in all_elements
 * @param my_elements Pointer to buffer where the local elements will be stored
 * @return Number of elements received by the current process
 */
int distribute_from_root(int* all_elements, int n, int** my_elements) {
    int myid, n_proc;
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    int block_size = n / n_proc;
    int remainder = n % n_proc;

    int start, length;

    if (myid < remainder) {
        start = myid * block_size + myid;
        length = block_size + 1;
    } else {
        start = myid * block_size + remainder;
        length = block_size;
    }

    MPI_Scatter(all_elements, length, MPI_INT, my_elements, length, MPI_INT, 0, MPI_COMM_WORLD);
    return length;
}

/**
 * Gather elements from all processes on root. Put root's elements first and
 * thereafter elements from the other nodes in the order of their ranks (so that
 * elements from process i come after the elements from process i-1).
 * @param all_elements Buffer on root where the elements will be stored
 * @param my_elements Elements to be gathered from the current process
 * @param local_n Number of elements in my_elements
 */
void gather_on_root(int* all_elements, int* my_elements, int local_n) {
    MPI_Gather(my_elements, local_n, MPI_INT, all_elements, local_n, MPI_INT, 0, MPI_COMM_WORLD);
}

/**
 * Perform the global part of parallel quick sort. This function assumes that
 * the elements is sorted within each node. When the function returns, all
 * elements owned by process i are smaller than or equal to all elements owned
 * by process i+1, and the elements are sorted within each node.
 * @param elements Pointer to the array of sorted values on the current node. Will point to a(n) (new) array with the sorted elements when the function returns.
 * @param n Length of *elements
 * @param MPI_Comm Communicator containing all processes participating in the global sort
 * @param pivot_strategy Tells how to select the pivot element. See documentation of select_pivot in pivot.h.
 * @return New length of *elements
 */
int global_sort(int** elements, int n, MPI_Comm, int pivot_strategy) {
    return 0;
}

/**
 * Merge v1 and v2 to one array, sorted in ascending order, and store the result
 * in result.
 * @param v1 Array to merge
 * @param n1 Length of v1
 * @param v2 Array to merge
 * @param n2 Length of v2
 * @param result Array for merged result (must be allocated before!)
 */
void merge_ascending(int* v1, int n1, int* v2, int n2, int* result) {
    int a = 0, b = 0, result_len = n1 + n2;
    for (int i = 0; i < result_len; i++) {
        if (a >= n1)
            result[i] = v2[b++];
        else if (b >= n2)
            result[i] = v1[a++];
        else if (v1[a] <= v2[b])
            result[i] = v1[a++];
        else
            result[i] = v2[b++];
    }
}

int read_input(char* file_name, int** elements) {
    FILE* file = fopen(file_name, "r");
    int n;
    fscanf(file, "%d", &n);
    if (n == 0) return 0;

    int* arr;
    arr = malloc(n * sizeof(int));
    elements = &arr;
    for (int i = 0; i < n; i++)
        fscanf(file, "%d", &arr[i]);

    fclose(file);
    return n;
}

int check_and_print(int* elements, int n, char* file_name) {
    if (sorted_ascending(elements, n)) {
        FILE* file = fopen(file_name, "w");
        for (int i = 0; i < n; i++) {
            fprintf(file, "%d ", elements[i]);
        }
        fclose(file);
        return 0;
    } else {
        return -2;
    };
}

void swap(int* e1, int* e2) {
    int temp = *e1;
    *e1 = *e2;
    *e2 = temp;
}

int sorted_ascending(int* elements, int n) {
    for (int i = 0; i < n - 1; i++) {
        if (elements[i] > elements[i + 1]) {
            printf("%d and %d are in the wrong order at indices %d and %d\n", elements[i], elements[i + 1], i, i + 1);
            return 0;
        }
    }
    return 1;
}
