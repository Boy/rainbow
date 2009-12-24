# -*- cmake -*-
include(Linking)
include(Prebuilt)

set(CARES_FIND_QUIETLY ON)
set(CARES_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindCARes)
else (STANDALONE)
    use_prebuilt_binary(ares)
    if (WINDOWS)
        set(CARES_LIBRARIES areslib)
    elseif (DARWIN)
        set(CARES_LIBRARIES
          optimized ${ARCH_PREBUILT_DIRS_RELEASE}/libcares.a
          debug ${ARCH_PREBUILT_DIRS_DEBUG}/libcares.a
          )
    else (WINDOWS)
        set(CARES_LIBRARIES cares)
    endif (WINDOWS)
    set(CARES_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/ares)
endif (STANDALONE)
