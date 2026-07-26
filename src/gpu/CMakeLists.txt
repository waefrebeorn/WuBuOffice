# ---------------------------------------------------------------------------
# Optional CUDA training backend (C11 + .cu kernels, nvcc).
# OFF by default so the project builds with no CUDA dependency. When ON and a
# CUDA toolkit is found, gpu_blas.cu (real kernels) is compiled; otherwise the
# CPU stub is used and gpu_available() returns 0. The on-disk model format and
# the C inference path are unaffected either way.
# ---------------------------------------------------------------------------
set(CUDA_LIBS "")
if(WITH_CUDA)
  find_program(NVCC_EXECUTABLE nvcc PATHS /usr/local/cuda/bin ENV PATH)
  if(NVCC_EXECUTABLE)
    message(STATUS "CUDA enabled: using ${NVCC_EXECUTABLE}")
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 11)
    # gpu_blas.cu is the real implementation; do NOT compile the stub.
    add_library(gpublas STATIC gpu_blas.cu)
    target_include_directories(gpublas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(gpublas PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:
        --gpu-architecture=sm_89 -O2>)
    find_library(CUDA_RUNTIME_LIB cuda HINTS /usr/local/cuda/targets/x86_64-linux/lib)
    set(CUDA_LIBS gpublas ${CUDA_RUNTIME_LIB})
    add_compile_definitions(WITH_CUDA_BUILD)
  else()
    message(WARNING "WITH_CUDA=ON but nvcc not found; falling back to CPU stub")
    add_library(gpublas STATIC gpu_blas_stub.c)
    target_include_directories(gpublas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    set(CUDA_LIBS gpublas)
  endif()
else()
  add_library(gpublas STATIC gpu_blas_stub.c)
  target_include_directories(gpublas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  set(CUDA_LIBS gpublas)
endif()
