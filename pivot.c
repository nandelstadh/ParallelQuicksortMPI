#include "pivot.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @param v1 Pointer to first value (integer) to compare
 * @param v2 Pointer to second value (integer) to compare
 * @return 0 if *v1==*v2, a positive and negative number of *v1>*v2 and *v1<*v2 respectively
 */
int compare(const void* v1, const void* v2) {
    double d1 = *(const double*)v1;
    double d2 = *(const double*)v1;
    return (d1 > d2) - (d1 < d2);
}

/**
 * Find the index of the first value in elements that is larger than val
 * @param elements Array to search in
 * @param n Length of elements
 * @param val Value to search for
 * @return index of first value that is larger than val, or length of array if all values are smaller
 */
int get_larger_index(int* elements, int n, int val) {
    for (int i = 0; i < n; i++) {
        if (i > val) return i;
    }
    return n;
}

/**
 * Find the median in an array. Note that this function assumes that the array
 * is sorted!
 * @param elements Sorted array
 * @param n Length of elements
 * @return median of elements
 */
int get_median(int* elements, int n) {
    return elements[n / 2];
}

/**
 * Select a pivot element for parallel quick sort. Return the index of the first
 * element that is larger than the pivot. Note that this function assumes that
 * elements is sorted!
 * @param pivot_strategy 0=>smallest on root (Not recommended!) 1=>median on root 2=>mean of medians 3=>median of medians
 * @param elements Elements stored by the current process (sorted!)
 * @param n Length of elements
 * @param communicator Communicator for processes in current group
 * @return The index of the first element after pivot
 */
/*We want to only call this from the root*/
int select_pivot(int pivot_strategy, int* elements, int n, MPI_Comm communicator) {
    int pivot;
    switch (pivot_strategy) {
        case 0:
            pivot = select_pivot_smallest_root(elements, n, communicator);
            return get_larger_index(elements, n, pivot);
        case 1:
            pivot = select_pivot_median_root(elements, n, communicator);
            return get_larger_index(elements, n, pivot);
        case 2:
            pivot = select_pivot_mean_median(elements, n, communicator);
            return get_larger_index(elements, n, pivot);
        case 3:
            pivot = select_pivot_median_median(elements, n, communicator);
            return get_larger_index(elements, n, pivot);
        default:
            printf("Invalid pivot selection\n");
            return -1;
    }
}

/*All of these should scatter the info to all the nodes straight away*/

/**
 * See select pivot!
 */
int select_pivot_median_root(int* elements, int n, MPI_Comm communicator) {
    int myid, pivot;
    MPI_Comm_rank(communicator, &myid);
    pivot = get_median(elements, n);
    MPI_Bcast(&pivot, 1, MPI_INT, 0, communicator);
    return pivot;
}

/**
 * See select pivot!
 */
int select_pivot_mean_median(int* elements, int n, MPI_Comm communicator) {
    int n_proc, pivot;
    int glob_pivot = -1;
    MPI_Comm_size(communicator, &n_proc);
    MPI_Reduce(&pivot, &pivot, 1, MPI_INT, MPI_SUM, 0, communicator);
    pivot = pivot / n_proc;
    MPI_Bcast(&pivot, 1, MPI_INT, 0, communicator);
    return pivot;
}

/**
 * See select pivot!
 */
int select_pivot_median_median(int* elements, int n, MPI_Comm communicator) {
    int n_proc, pivot, myid;
    MPI_Comm_size(communicator, &n_proc);
    MPI_Comm_rank(communicator, &myid);
    int pivots[n_proc];
    MPI_Gather(&pivot, 1, MPI_INT, pivots, 1, MPI_INT, 0, communicator);
    qsort(pivots, n_proc, sizeof(int), compare);
    pivot = pivots[n_proc / 2];
    MPI_Bcast(&pivot, 1, MPI_INT, 0, communicator);
    return pivot;
}

/**
 * See select pivot!
 */
int select_pivot_smallest_root(int* elements, int n, MPI_Comm communicator) {
    int myid, pivot;
    MPI_Comm_rank(communicator, &myid);
    pivot = elements[0];
    MPI_Bcast(&pivot, 1, MPI_INT, 0, communicator);
    return pivot;
}
