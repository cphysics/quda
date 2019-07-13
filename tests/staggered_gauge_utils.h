#pragma once
#include <blas_reference.h>
#include <quda_internal.h>
#include "color_spinor_field.h"

extern int Z[4];
extern int Vh;
extern int V;

using namespace quda;

void setDims(int *);

// Wrap everything for the GPU construction of fat/long links here
void computeHISQLinksGPU(void** qdp_fatlink, void** qdp_longlink,
			 void** qdp_fatlink_eps, void** qdp_longlink_eps,
			 void** qdp_inlink, QudaGaugeParam &gauge_param,
			 double** act_path_coeffs, double eps_naik,
			 size_t gSize, int n_naiks);

void computeFatLong(void** qdp_fatlink, void** qdp_longlink,
		    void** qdp_inlink, QudaGaugeParam &gauge_param,
		    size_t gSize, int n_naiks, double eps_naik);
