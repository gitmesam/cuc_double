/*
   -- MAGMA (version 2.2.0) --
   Univ. of Tennessee, Knoxville
   Univ. of California, Berkeley
   Univ. of Colorado, Denver
   @date November 2016

   @author Azzam Haidar
   @author Tingxing Dong

   @generated from src/zgetf2_batched.cpp, normal z -> d, Sun Nov 20 20:20:26 2016
*/
#include "magma_internal.h"
#include "batched_kernel_param.h"

/***************************************************************************//**
    Purpose
    -------
    DGETF2 computes an LU factorization of a general M-by-N matrix A
    using partial pivoting with row interchanges.

    The factorization has the form
        A = P * L * U
    where P is a permutation matrix, L is lower triangular with unit
    diagonal elements (lower trapezoidal if m > n), and U is upper
    triangular (upper trapezoidal if m < n).

    This is the right-looking Level 3 BLAS version of the algorithm.

    This is a batched version that factors batchCount M-by-N matrices in parallel.
    dA, ipiv, and info become arrays with one entry per matrix.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of each matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of each matrix A.  N >= 0.

    @param[in,out]
    dA_array    Array of pointers, dimension (batchCount).
            Each is a DOUBLE PRECISION array on the GPU, dimension (LDDA,N).
            On entry, each pointer is an M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    @param[in]
    ldda    INTEGER
            The leading dimension of each array A.  LDDA >= max(1,M).

    @param
    dW0_displ (workspace) Array of pointers, dimension (batchCount).
    
    @param
    dW1_displ (workspace) Array of pointers, dimension (batchCount).

    @param
    dW2_displ (workspace) Array of pointers, dimension (batchCount).


    @param[out]
    ipiv_array  Array of pointers, dimension (batchCount), for corresponding matrices.
            Each is an INTEGER array, dimension (min(M,N))
            The pivot indices; for 1 <= i <= min(M,N), row i of the
            matrix was interchanged with row IPIV(i).

    @param[out]
    info_array  Array of INTEGERs, dimension (batchCount), for corresponding matrices.
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
                  or another error occured, such as memory allocation failed.
      -     > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
                  has been completed, but the factor U is exactly
                  singular, and division by zero will occur if it is used
                  to solve a system of equations.

    @param[in]
    gbstep  INTEGER
            internal use.

    @param[in]
    batchCount  INTEGER
                The number of matrices to operate on.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    this is an internal routine that might have many assumption.


    @ingroup magma_getf2_batched
*******************************************************************************/
extern "C" magma_int_t
magma_dgetf2_batched(
    magma_int_t m, magma_int_t n,
    double **dA_array, magma_int_t ldda,
    double **dW0_displ,
    double **dW1_displ,
    double **dW2_displ,
    magma_int_t **ipiv_array,
    magma_int_t *info_array,
    magma_int_t gbstep,
    magma_int_t batchCount,
    magma_queue_t queue)
{
    #define A(i, j)  (A + (i) + (j)*ldda)
    
    magma_int_t arginfo = 0;
    if (m < 0) {
        arginfo = -1;
    } else if (n < 0 ) {
        arginfo = -2;
    } else if (ldda < max(1,m)) {
        arginfo = -4;
    }

    if (arginfo != 0) {
        magma_xerbla( __func__, -(arginfo) );
        return arginfo;
    }

    // Quick return if possible
    if (m == 0 || n == 0) {
        return arginfo;
    }

    double c_neg_one = MAGMA_D_NEG_ONE;
    double c_one     = MAGMA_D_ONE;
    magma_int_t nb = BATF2_NB;

    

    magma_int_t min_mn = min(m, n);
    magma_int_t gbj, panelj, step, ib;

    for( panelj=0; panelj < min_mn; panelj += nb)
    {
        ib = min(nb, min_mn-panelj);

        for (step=0; step < ib; step++) {
            gbj = panelj+step;
            //size_t required_shmem_size = zamax*(sizeof(double)+sizeof(int)) + (m-panelj+2)*sizeof(double);
            //if ( (m-panelj) > 0)
            if ( (m-panelj) > MAX_NTHREADS)
            //if ( required_shmem_size >  (MAX_SHARED_ALLOWED*1024))
            {
                //printf("running non shared version\n");
                // find the max of the column gbj
                arginfo = magma_idamax_batched(m-gbj, dA_array, 1, gbj, ldda, ipiv_array, info_array, gbstep, batchCount, queue);
                if (arginfo != 0 ) return arginfo;
                // Apply the interchange to columns 1:N. swap the whole row
                arginfo = magma_dswap_batched(n, dA_array, ldda, gbj, ipiv_array, batchCount, queue);
                if (arginfo != 0 ) return arginfo;
                // Compute elements J+1:M of J-th column.
                if (gbj < m) {
                    arginfo = magma_dscal_dger_batched( m-gbj, ib-step, gbj, dA_array, ldda, info_array, gbstep, batchCount, queue );
                    if (arginfo != 0 ) return arginfo;
                }
            }
            else {
                //printf("running --- shared version\n");
                arginfo = magma_dcomputecolumn_batched(m-panelj, panelj, step, dA_array, ldda, ipiv_array, info_array, gbstep, batchCount, queue);
                if (arginfo != 0 ) return arginfo;
                // Apply the interchange to columns 1:N. swap the whole row
                arginfo = magma_dswap_batched(n, dA_array, ldda, gbj, ipiv_array, batchCount, queue);
                if (arginfo != 0 ) return arginfo;
            }
        }


        if ( (n-panelj-ib) > 0) {
            // continue the update of the selected ib row column panelj+ib:n(TRSM)
            magma_dgetf2trsm_batched(ib, n-panelj-ib, dA_array, panelj, ldda, batchCount, queue);
            // do the blocked DGER = DGEMM for the remaining panelj+ib:n columns
            magma_ddisplace_pointers(dW0_displ, dA_array, ldda, ib+panelj, panelj, batchCount, queue);
            magma_ddisplace_pointers(dW1_displ, dA_array, ldda, panelj, ib+panelj, batchCount, queue);
            magma_ddisplace_pointers(dW2_displ, dA_array, ldda, ib+panelj, ib+panelj, batchCount, queue);

            magma_dgemm_batched( MagmaNoTrans, MagmaNoTrans, m-(panelj+ib), n-(panelj+ib), ib,
                                 c_neg_one, dW0_displ, ldda,
                                            dW1_displ, ldda,
                                 c_one,     dW2_displ, ldda,
                                 batchCount, queue );
        }
    }

    //magma_free_cpu(cpuAarray);

    return 0;
}
