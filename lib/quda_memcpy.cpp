#include <tune_quda.h>
#include <uint_to_char.h>

namespace quda {

  class QudaMemCopy : public Tunable {

    void *dst;
    const void *src;
    const size_t count;
    const cudaMemcpyKind kind;
    const bool async;
    const char *name;

    unsigned int sharedBytesPerThread() const { return 0; }
    unsigned int sharedBytesPerBlock(const TuneParam &param) const { return 0; }

  public:
    inline QudaMemCopy(void *dst, const void *src, size_t count, cudaMemcpyKind kind,
                       bool async, const char *func, const char *file, const char *line)
      : dst(dst), src(src), count(count), kind(kind), async(async) {

      if (!async) {
        switch (kind) {
        case cudaMemcpyDeviceToHost:   name = "cudaMemcpyDeviceToHost";   break;
        case cudaMemcpyHostToDevice:   name = "cudaMemcpyHostToDevice";   break;
        case cudaMemcpyHostToHost:     name = "cudaMemcpyHostToHost";     break;
        case cudaMemcpyDeviceToDevice: name = "cudaMemcpyDeviceToDevice"; break;
        case cudaMemcpyDefault:        name = "cudaMemcpyDefault";        break;
        default: errorQuda("Unsupported cudaMemcpyType %d", kind);
        }
      } else {
        switch(kind) {
        case cudaMemcpyDeviceToHost:   name = "cudaMemcpyAsyncDeviceToHost";   break;
        case cudaMemcpyHostToDevice:   name = "cudaMemcpyAsyncHostToDevice";   break;
        case cudaMemcpyHostToHost:     name = "cudaMemcpyAsyncHostToHost";     break;
        case cudaMemcpyDeviceToDevice: name = "cudaMemcpyAsyncDeviceToDevice"; break;
        case cudaMemcpyDefault:        name = "cudaMemcpyAsyncDefault";        break;
        default: errorQuda("Unsupported cudaMemcpyType %d", kind);
        }
      }
      strcpy(aux, func);
      strcat(aux, ",");
      strcat(aux, file);
      strcat(aux, ",");
      strcat(aux, line);
    }

    virtual ~QudaMemCopy() { }

    inline void apply(const cudaStream_t &stream) {
      tuneLaunch(*this, getTuning(), getVerbosity());
      if (async) cudaMemcpyAsync(dst, src, count, kind, stream);
      else cudaMemcpy(dst, src, count, kind);
    }

    bool advanceTuneParam(TuneParam &param) const { return false; }

    TuneKey tuneKey() const {
      char vol[128];
      strcpy(vol,"bytes=");
      u64toa(vol+6, (uint64_t)count);
      return TuneKey(vol, name, aux);
    }

    long long flops() const { return 0; }
    long long bytes() const { return kind == cudaMemcpyDeviceToDevice ? 2*count : count; }

  };

  void qudaMemcpy_(void *dst, const void *src, size_t count, cudaMemcpyKind kind,
                   const char *func, const char *file, const char *line) {
    if (getVerbosity() == QUDA_DEBUG_VERBOSE)
      printfQuda("%s bytes = %llu\n", __func__, (long long unsigned int)count);

    if (count == 0) return;
#if 1
    QudaMemCopy copy(dst, src, count, kind, false, func, file, line);
    copy.apply(0);
#else
    cudaMemcpy(dst, src, count, kind);
#endif
    checkCudaError();
  }

  void qudaMemcpyAsync_(void *dst, const void *src, size_t count, cudaMemcpyKind kind, const cudaStream_t &stream,
                        const char *func, const char *file, const char *line) {
    if (getVerbosity() == QUDA_DEBUG_VERBOSE)
      printfQuda("%s bytes = %llu\n", __func__, (long long unsigned int)count);

    if (count == 0) return;
#if 1
    QudaMemCopy copy(dst, src, count, kind, true, func, file, line);
    copy.apply(stream);
#else
    cudaMemcpyAsync(dst, src, count, kind, stream);
#endif
    checkCudaError();
  }

} // namespace quda
