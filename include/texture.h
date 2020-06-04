#pragma once

/**
   Checks that the types are set correctly.  The precision used in the
   RegType must match that of the InterType, and the ordering of the
   InterType must match that of the StoreType.  The only exception is
   when fixed precision is used, in which case, RegType can be a double
   and InterType can be single (with StoreType short or char).

   @param RegType Register type used in kernel
   @param InterType Intermediate format - RegType precision with StoreType ordering
   @param StoreType Type used to store field in memory
*/
template <typename RegType, typename InterType, typename StoreType> void checkTypes()
{
  const size_t reg_size = sizeof(((RegType *)0)->x);
  const size_t inter_size = sizeof(((InterType *)0)->x);
  const size_t store_size = sizeof(((StoreType *)0)->x);

  if (reg_size != inter_size && store_size != 2 && store_size != 1 && inter_size != 4)
    errorQuda("Precision of register (%lu) and intermediate (%lu) types must match\n", (unsigned long)reg_size,
        (unsigned long)inter_size);

  if (vec_length<InterType>::value != vec_length<StoreType>::value) {
    errorQuda("Vector lengths intermediate and register types must match\n");
  }

  if (vec_length<RegType>::value == 0) errorQuda("Vector type not supported\n");
  if (vec_length<InterType>::value == 0) errorQuda("Vector type not supported\n");
  if (vec_length<StoreType>::value == 0) errorQuda("Vector type not supported\n");
}

template <typename RegType, typename StoreType, bool is_fixed> struct SpinorNorm {
  using InterType = typename bridge_mapper<RegType, StoreType>::type;
  float *norm;
  unsigned int cb_norm_offset;

  SpinorNorm() : norm(nullptr), cb_norm_offset(0) {}

  SpinorNorm(const ColorSpinorField &x) : norm((float *)x.Norm()), cb_norm_offset(x.NormBytes() / (2 * sizeof(float)))
  {
  }

  SpinorNorm(const SpinorNorm &sn) : norm(sn.norm), cb_norm_offset(sn.cb_norm_offset) {}

  SpinorNorm &operator=(const SpinorNorm &src)
  {
    if (&src != this) {
      norm = src.norm;
      cb_norm_offset = src.cb_norm_offset;
    }
    return *this;
  }

  void set(const ColorSpinorField &x)
  {
    norm = (float *)x.Norm();
    cb_norm_offset = x.NormBytes() / (2 * sizeof(float));
  }

  __device__ inline float load_norm(const int i, const int parity = 0) const { return norm[cb_norm_offset * parity + i]; }

  template <int M> __device__ inline float store_norm(InterType x[M], int i, int parity)
  {
    float c[M/2];
#pragma unroll
    for (int j = 0; j < M/2; j++) c[j] = fmaxf(max_fabs(x[2*j+0]), max_fabs(x[2*j+1]));
#pragma unroll
    for (int j = 1; j < M/2; j++) c[0] = fmaxf(c[j], c[0]);
    norm[cb_norm_offset * parity + i] = c[0];
    return __fdividef(fixedMaxValue<StoreType>::value, c[0]);
  }

  float *Norm() { return norm; }
};

template <typename RegType, typename StoreType> struct SpinorNorm<RegType, StoreType, false> {
  typedef typename bridge_mapper<RegType, StoreType>::type InterType;
  SpinorNorm() {}
  SpinorNorm(const ColorSpinorField &x) {}
  SpinorNorm(const SpinorNorm &sn) {}
  SpinorNorm &operator=(const SpinorNorm &src) { return *this; }
  void set(const ColorSpinorField &x) {}
  __device__ inline float load_norm(const int i, const int parity = 0) const { return 1.0; }
  template <int M> __device__ inline float store_norm(InterType x[M], int i, int parity) { return 1.0; }
  void backup(char **norm_h, size_t norm_bytes) {}
  void restore(char **norm_h, size_t norm_bytes) {}
  float *Norm() { return nullptr; }
};

/**
   @param RegType Register type used in kernel
   @param InterType Intermediate format - RegType precision with StoreType ordering
   @param StoreType Type used to store field in memory
   @param N Length of vector of RegType elements that this Spinor represents
*/
template <typename RegType_, typename StoreType_, int N>
struct Spinor : SpinorNorm<RegType_, StoreType_, isFixed<StoreType_>::value> {
  typedef RegType_ RegType;
  typedef StoreType_ StoreType;
  typedef typename bridge_mapper<RegType,StoreType>::type InterType;
  typedef SpinorNorm<RegType, StoreType_, isFixed<StoreType>::value> SN;

  StoreType *spinor;
  int stride;
  unsigned int cb_offset;

public:
  Spinor() :
    SN(),
    spinor(nullptr),
    stride(0),
    cb_offset(0)
  { }

 Spinor(const ColorSpinorField &x) :
    SN(x),
    spinor(static_cast<StoreType*>(const_cast<ColorSpinorField &>(x).V())),
    stride(x.Stride()),
    cb_offset(x.Bytes() / (2 * sizeof(StoreType)))
  {
    checkTypes<RegType, InterType, StoreType>();
  }

  Spinor(const Spinor &st) :
    SN(st),
    spinor(st.spinor),
    stride(st.stride),
    cb_offset(st.cb_offset)
  {
  }

  Spinor &operator=(const Spinor &src)
  {
    if (&src != this) {
      SN::operator=(src);
      spinor = src.spinor;
      stride = src.stride;
      cb_offset = src.cb_offset;
    }
    return *this;
  }

  void set(const ColorSpinorField &x)
  {
    SN::set(x);
    spinor = static_cast<StoreType*>(const_cast<ColorSpinorField &>(x).V());
    stride = x.Stride();
    cb_offset = x.Bytes() / (2 * sizeof(StoreType));
    checkTypes<RegType, InterType, StoreType>();
  }

  __device__ inline void load(RegType x[], const int i, const int parity = 0) const
  {
    // load data into registers first using the storage order
    constexpr int M = (N * vec_length<RegType>::value) / vec_length<InterType>::value;
    InterType y[M];

    // fixed precision
    if (isFixed<StoreType>::value) {
      const float xN = SN::load_norm(i, parity);
#pragma unroll
      for (int j = 0; j < M; j++) copy_and_scale(y[j], spinor[cb_offset * parity + i + j * stride], xN);
    } else { // other types
#pragma unroll
      for (int j = 0; j < M; j++) copyFloatN(y[j], spinor[cb_offset * parity + i + j * stride]);
    }

    // now convert into desired register order
    convert<RegType, InterType>(x, y, N);
  }

  __device__ inline void save(RegType x[], int i, const int parity = 0)
  {
    constexpr int M = (N * vec_length<RegType>::value) / vec_length<InterType>::value;
    InterType y[M];
    convert<InterType, RegType>(y, x, M);

    if (isFixed<StoreType>::value) {
      float C = SN::template store_norm<M>(y, i, parity);
#pragma unroll
      for (int j = 0; j < M; j++) copyFloatN(spinor[cb_offset * parity + i + j * stride], C * y[j]);
    } else {
#pragma unroll
      for (int j = 0; j < M; j++) copyFloatN(spinor[cb_offset * parity + i + j * stride], y[j]);
    }
  }

};
