#include "quicksort.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pivot.h"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Expected: quicksort input output pivot\n");
        return -1;
    }
    char* input = argv[1];
    char* output = argv[2];
    int pivot_strategy = atoi(argv[3]);
    int* elements;
    int* my_elements;
    int n, local_n, myid, n_proc;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    n = read_input(input, &elements);
    local_n = distribute_from_root(elements, n, &my_elements);

    double start = MPI_Wtime();
    qsort(my_elements, local_n, sizeof(int), compare);
    local_n = global_sort(&my_elements, local_n, MPI_COMM_WORLD, pivot_strategy);
    double time = MPI_Wtime() - start;
    double max_time;
    MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    gather_on_root(elements, my_elements, local_n);
    if (myid == 0) {
        printf("%f\n", max_time);
        check_and_print(elements, n, output);
    }

    free(elements);
    free(my_elements);
    MPI_Finalize();
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

    int counts[n_proc];
    int displs[n_proc];

    for (int id = 0; id < n_proc; id++) {
        if (id < remainder) {
            displs[id] = id * block_size + id;
            counts[id] = block_size + 1;
        } else {
            displs[id] = id * block_size + remainder;
            counts[id] = block_size;
        }
    }

    *my_elements = malloc(sizeof(int) * counts[myid]);
    MPI_Scatterv(all_elements, counts, displs, MPI_INT, *my_elements, counts[myid], MPI_INT, 0, MPI_COMM_WORLD);
    return counts[myid];
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
    int myid, n_proc;
    MPI_Comm_size(MPI_COMM_WORLD, &n_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    int counts[n_proc];
    int displs[n_proc];
    displs[0] = 0;

    MPI_Gather(&local_n, 1, MPI_INT, counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (myid == 0) {
        for (int i = 1; i < n_proc; i++) {
            displs[i] = displs[i - 1] + counts[i - 1];
        }
    }
    MPI_Gatherv(my_elements, local_n, MPI_INT, all_elements, counts, displs, MPI_INT, 0, MPI_COMM_WORLD);
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
int global_sort(int** elements, int n, MPI_Comm communicator, int pivot_strategy) {
    int n_proc, myid;
    MPI_Comm_size(communicator, &n_proc);
    MPI_Comm_rank(communicator, &myid);
    if (n_proc == 1) return n;

    /* We select our pivot and get its index*/
    int idx = select_pivot(pivot_strategy, *elements, n, communicator);

    /*Split data into larger and smaller*/
    int* v1 = *elements;
    int* v2 = (*elements) + idx;

    int n1 = idx;
    int n2 = n - idx;

    /*Exchange data pairwise between processors*/
    int half = n_proc / 2;
    int partner = (myid + half) % n_proc;

    // First we need to exchange list and pivot data
    int data[3] = {n1, n2, idx};
    int partner_data[3];
    MPI_Sendrecv(data, 3, MPI_INT, partner, 0, partner_data, 3, MPI_INT, partner, 0, communicator, MPI_STATUS_IGNORE);

    // Then we can actually exchange the lists
    int* buf;
    if (myid < half) {
        int buf_n = partner_data[0];
        buf = malloc(sizeof(int) * buf_n);
        MPI_Sendrecv(v2, n2, MPI_INT, partner, 0, buf, buf_n, MPI_INT, partner, 0, communicator, MPI_STATUS_IGNORE);
        n2 = buf_n;
        v2 = buf;
    } else {
        int buf_n = partner_data[1];
        buf = malloc(sizeof(int) * buf_n);
        MPI_Sendrecv(v1, n1, MPI_INT, partner, 0, buf, buf_n, MPI_INT, partner, 0, communicator, MPI_STATUS_IGNORE);
        n1 = buf_n;
        v1 = buf;
    }

    /*Merge the data into one list*/
    int* merged;
    merged = malloc((n1 + n2) * sizeof(int));
    merge_ascending(v1, n1, v2, n2, merged);
    *elements = merged;

    /*Recurse*/
    int color;

    if (myid < n_proc / 2) {
        color = 0;
    } else {
        color = 1;
    }

    MPI_Comm recursion_comm;
    MPI_Comm_split(communicator, color, myid, &recursion_comm);
    n = global_sort(elements, n1 + n2, recursion_comm, pivot_strategy);
    free(buf);
    MPI_Comm_free(&recursion_comm);
    return n;
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
    for (int i = 0; i < n; i++)
        fscanf(file, "%d", &arr[i]);

    *elements = arr;
    fclose(file);
    return n;
}

int check_and_print(int* elements, int n, char* file_name) {
    if (sorted_ascending(elements, n)) {
        FILE* file = fopen(file_name, "w");
        for (int i = 0; i < n; i++) {
            fprintf(file, "%d ", elements[i]);
        }
        fprintf(file, "\n");
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
