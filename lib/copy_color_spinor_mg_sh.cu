#include <copy_color_spinor_mg.cuh>

namespace quda {
  
  void copyGenericColorSpinorMGSH(ColorSpinorField &dst, const ColorSpinorField &src, 
				  QudaFieldLocation location, void *Dst, void *Src, 
				  void *dstNorm, void *srcNorm) {

#if defined(GPU_MULTIGRID)
    auto *dst_ptr = static_cast<float*>(Dst);
    auto *src_ptr = static_cast<short*>(Src);

    INSTANTIATE_COLOR;
#else
    errorQuda("Double precision multigrid has not been enabled");
#endif

  }

} // namespace quda
