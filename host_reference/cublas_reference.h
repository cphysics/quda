#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <complex>
#include <inttypes.h>

#include <util_quda.h>
#include <host_utils.h>
#include <command_line_params.h>
#include "misc.h"

#include <Eigen/Dense>
using namespace Eigen;

using namespace std;

void fillEigenArrayColMaj(MatrixXcd &EigenArr, complex<double> *arr, int rows, int cols, int ld, int offset);

void fillEigenArrayRowMaj(MatrixXcd &EigenArr, complex<double> *arr, int rows, int cols, int ld, int offset);
void cublasGEMMEigenVerify(void *arrayA, void *arrayB, void *arrayCcopy, void *arrayC, QudaCublasParam *cublas_param);

void cublasGEMMQudaVerify(void *arrayA, void *arrayB, void *arrayC, void *arrayCcopy,
			  uint64_t refA_size, uint64_t refB_size, uint64_t refC_size,
			  int re_im, size_t data_size,
			  QudaCublasParam *cublas_param);