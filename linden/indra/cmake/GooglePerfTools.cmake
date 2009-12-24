# -*- cmake -*-
include(Prebuilt)

if (STANDALONE)
  include(FindGooglePerfTools)
else (STANDALONE)
  use_prebuilt_binary(google)
  if (LINUX)
    set(TCMALLOC_LIBRARIES tcmalloc)
    set(STACKTRACE_LIBRARIES stacktrace)
    set(PROFILER_LIBRARIES profiler)
    set(GOOGLE_PERFTOOLS_INCLUDE_DIR
        ${LIBS_PREBUILT_DIR}/${LL_ARCH_DIR}/include)
    set(GOOGLE_PERFTOOLS_FOUND "YES")
  endif (LINUX)
endif (STANDALONE)

if (GOOGLE_PERFTOOLS_FOUND)
  set(USE_GOOGLE_PERFTOOLS ON CACHE BOOL "Build with Google PerfTools support.")
endif (GOOGLE_PERFTOOLS_FOUND)

# XXX Disable temporarily, until we have compilation issues on 64-bit
# Etch sorted.
set(USE_GOOGLE_PERFTOOLS OFF)

if (USE_GOOGLE_PERFTOOLS)
  set(TCMALLOC_FLAG -DLL_USE_TCMALLOC=1)
  include_directories(${GOOGLE_PERFTOOLS_INCLUDE_DIR})
  set(GOOGLE_PERFTOOLS_LIBRARIES ${TCMALLOC_LIBRARIES} ${STACKTRACE_LIBRARIES} ${PROFILER_LIBRARIES})
else (USE_GOOGLE_PERFTOOLS)
  set(TCMALLOC_FLAG -ULL_USE_TCMALLOC)
endif (USE_GOOGLE_PERFTOOLS)
