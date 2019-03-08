#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <test_util.h>
#include <quda_internal.h>
#include <quda.h>
#include <util_quda.h>
#include <staggered_dslash_reference.h>
#include "misc.h"
#include <blas_quda.h>

#include <blas_reference.h>

extern void *memset(void *s, int c, size_t n);

#include <dslash_util.h>

//
// dslashReference()
//
// if oddBit is zero: calculate even parity spinor elements (using odd parity spinor)
// if oddBit is one:  calculate odd parity spinor elements
//
// if daggerBit is zero: perform ordinary dslash operator
// if daggerBit is one:  perform hermitian conjugate of dslash
//
template<typename Float>
void display_link_internal(Float* link)
{
  int i, j;

  for (i = 0;i < 3; i++){
    for(j=0;j < 3; j++){
      printf("(%10f,%10f) \t", link[i*3*2 + j*2], link[i*3*2 + j*2 + 1]);
    }
    printf("\n");
  }
  printf("\n");
  return;
}


template <typename sFloat, typename gFloat>
void dslashReference(sFloat *res, gFloat **fatlink, gFloat** longlink,
                     gFloat** ghostFatlink, gFloat** ghostLonglink,
                     sFloat *spinorField, sFloat** fwd_nbr_spinor,
                     sFloat** back_nbr_spinor, int oddBit, int daggerBit,
                     int nSrc, QudaDslashType dslash_type)
{
  for (int i=0; i<Vh*mySpinorSiteSize*nSrc; i++) res[i] = 0.0;

  gFloat *fatlinkEven[4], *fatlinkOdd[4];
  gFloat *longlinkEven[4], *longlinkOdd[4];
  gFloat *ghostFatlinkEven[4], *ghostFatlinkOdd[4];
  gFloat *ghostLonglinkEven[4], *ghostLonglinkOdd[4];

  for (int dir = 0; dir < 4; dir++) {
    fatlinkEven[dir] = fatlink[dir];
    fatlinkOdd[dir] = fatlink[dir] + Vh*gaugeSiteSize;
    longlinkEven[dir] =longlink[dir];
    longlinkOdd[dir] = longlink[dir] + Vh*gaugeSiteSize;

    ghostFatlinkEven[dir] = ghostFatlink[dir];
    ghostFatlinkOdd[dir] = ghostFatlink[dir] + (faceVolume[dir]/2)*gaugeSiteSize;
    ghostLonglinkEven[dir] = ghostLonglink[dir];
    ghostLonglinkOdd[dir] = ghostLonglink[dir] + 3*(faceVolume[dir]/2)*gaugeSiteSize;
  }

  for (int xs=0; xs<nSrc; xs++) {

    for (int i = 0; i < Vh; i++) {
      int sid = i + xs*Vh;
      int offset = mySpinorSiteSize*sid;

      for (int dir = 0; dir < 8; dir++) {
#ifdef MULTI_GPU
        const int nFace = dslash_type == QUDA_ASQTAD_DSLASH ? 3 : 1;
        gFloat* fatlnk = gaugeLink_mg4dir(i, dir, oddBit, fatlinkEven, fatlinkOdd, ghostFatlinkEven, ghostFatlinkOdd, 1, 1);
        gFloat* longlnk = dslash_type == QUDA_ASQTAD_DSLASH ? gaugeLink_mg4dir(i, dir, oddBit, longlinkEven, longlinkOdd, ghostLonglinkEven, ghostLonglinkOdd, 3, 3) : nullptr;
        sFloat *first_neighbor_spinor = spinorNeighbor_5d_mgpu<QUDA_4D_PC>(sid, dir, oddBit, spinorField, fwd_nbr_spinor, back_nbr_spinor, 1, nFace, mySpinorSiteSize);
        sFloat *third_neighbor_spinor = dslash_type == QUDA_ASQTAD_DSLASH ? spinorNeighbor_5d_mgpu<QUDA_4D_PC>(sid, dir, oddBit, spinorField, fwd_nbr_spinor, back_nbr_spinor, 3, nFace, mySpinorSiteSize) : nullptr;
#else
        gFloat* fatlnk = gaugeLink(i, dir, oddBit, fatlinkEven, fatlinkOdd, 1);
        gFloat* longlnk = dslash_type == QUDA_ASQTAD_DSLASH ? gaugeLink(i, dir, oddBit, longlinkEven, longlinkOdd, 3) : nullptr;
        sFloat *first_neighbor_spinor = spinorNeighbor_5d<QUDA_4D_PC>(sid, dir, oddBit, spinorField, 1, mySpinorSiteSize);
        sFloat *third_neighbor_spinor = dslash_type == QUDA_ASQTAD_DSLASH ? spinorNeighbor_5d<QUDA_4D_PC>(sid, dir, oddBit, spinorField, 3, mySpinorSiteSize) : nullptr;
#endif
        sFloat gaugedSpinor[mySpinorSiteSize];

        if (dir % 2 == 0){
          su3Mul(gaugedSpinor, fatlnk, first_neighbor_spinor);
          sum(&res[offset], &res[offset], gaugedSpinor, mySpinorSiteSize);

          if (dslash_type == QUDA_ASQTAD_DSLASH) {
            su3Mul(gaugedSpinor, longlnk, third_neighbor_spinor);
            sum(&res[offset], &res[offset], gaugedSpinor, mySpinorSiteSize);
          }
        } else {
          su3Tmul(gaugedSpinor, fatlnk, first_neighbor_spinor);
          if (dslash_type == QUDA_LAPLACE_DSLASH) {
            sum(&res[offset], &res[offset], gaugedSpinor, mySpinorSiteSize);
          } else {
            sub(&res[offset], &res[offset], gaugedSpinor, mySpinorSiteSize);
          }

          if (dslash_type == QUDA_ASQTAD_DSLASH) {
            su3Tmul(gaugedSpinor, longlnk, third_neighbor_spinor);
            sub(&res[offset], &res[offset], gaugedSpinor, mySpinorSiteSize);
          }
        }
      }

      if (daggerBit) negx(&res[offset], mySpinorSiteSize);
    } // 4-d volume
  } // right-hand-side

}



void staggered_dslash(cpuColorSpinorField* out, void **fatlink, void** longlink, void** ghost_fatlink,
                      void** ghost_longlink, cpuColorSpinorField* in, int oddBit, int daggerBit,
                      QudaPrecision sPrecision, QudaPrecision gPrecision, QudaDslashType dslash_type)
{
  const int nSrc = in->X(4);

  QudaParity otherparity = QUDA_INVALID_PARITY;
  if (oddBit == QUDA_EVEN_PARITY) {
    otherparity = QUDA_ODD_PARITY;
  } else if (oddBit == QUDA_ODD_PARITY) {
    otherparity = QUDA_EVEN_PARITY;
  } else {
    errorQuda("ERROR: full parity not supported in function %s", __FUNCTION__);
  }
  const int nFace = dslash_type == QUDA_ASQTAD_DSLASH ? 3 : 1;

  in->exchangeGhost(otherparity, nFace, daggerBit);

  void** fwd_nbr_spinor = in->fwdGhostFaceBuffer;
  void** back_nbr_spinor = in->backGhostFaceBuffer;

  if (sPrecision == QUDA_DOUBLE_PRECISION) {
    if (gPrecision == QUDA_DOUBLE_PRECISION) {
      dslashReference((double*)out->V(), (double**)fatlink, (double**)longlink, (double**)ghost_fatlink, (double**)ghost_longlink,
                      (double*)in->V(), (double**)fwd_nbr_spinor, (double**)back_nbr_spinor, oddBit, daggerBit, nSrc, dslash_type);
    } else {
      dslashReference((double*)out->V(), (float**)fatlink, (float**)longlink, (float**)ghost_fatlink, (float**)ghost_longlink,
                      (double*)in->V(), (double**)fwd_nbr_spinor, (double**)back_nbr_spinor, oddBit, daggerBit, nSrc, dslash_type);
      }
  } else {
    if (gPrecision == QUDA_DOUBLE_PRECISION) {
      dslashReference((float*)out->V(), (double**)fatlink, (double**)longlink, (double**)ghost_fatlink, (double**)ghost_longlink,
                      (float*)in->V(), (float**)fwd_nbr_spinor, (float**)back_nbr_spinor, oddBit, daggerBit, nSrc, dslash_type);
    } else {
      dslashReference((float*)out->V(), (float**)fatlink, (float**)longlink, (float**)ghost_fatlink, (float**)ghost_longlink,
                      (float*)in->V(), (float**)fwd_nbr_spinor, (float**)back_nbr_spinor, oddBit, daggerBit, nSrc, dslash_type);
    }
  }

}

void matdagmat(cpuColorSpinorField* out, void **fatlink, void** longlink, void** ghost_fatlink, void** ghost_longlink,
               cpuColorSpinorField* in, double mass, int dagger_bit,
               QudaPrecision sPrecision, QudaPrecision gPrecision, cpuColorSpinorField* tmp, QudaParity parity, QudaDslashType dslash_type)
{
  //assert sPrecision and gPrecision must be the same
  if (sPrecision != gPrecision){
    errorQuda("Spinor precision and gPrecison is not the same");
  }

  QudaParity otherparity = QUDA_INVALID_PARITY;
  if (parity == QUDA_EVEN_PARITY){
    otherparity = QUDA_ODD_PARITY;
  } else if (parity == QUDA_ODD_PARITY) {
    otherparity = QUDA_EVEN_PARITY;
  } else {
    errorQuda("ERROR: full parity not supported in function %s\n", __FUNCTION__);
  }

  staggered_dslash(tmp, fatlink, longlink, ghost_fatlink, ghost_longlink,
                   in, otherparity, dagger_bit, sPrecision, gPrecision, dslash_type);

  staggered_dslash(out, fatlink, longlink, ghost_fatlink, ghost_longlink,
                   tmp, parity, dagger_bit, sPrecision, gPrecision, dslash_type);

  double msq_x4 = mass*mass*4;
  if (sPrecision == QUDA_DOUBLE_PRECISION){
    axmy((double*)in->V(), (double)msq_x4, (double*)out->V(), out->X(4)*Vh*mySpinorSiteSize);
  }else{
    axmy((float*)in->V(), (float)msq_x4, (float*)out->V(), out->X(4)*Vh*mySpinorSiteSize);
  }

}
