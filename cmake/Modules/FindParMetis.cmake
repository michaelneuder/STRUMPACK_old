if (NOT (PARMETIS_INCLUDES AND PARMETIS_LIBRARIES))
  find_path(PARMETIS_INCLUDES NAMES parmetis.h PATHS $ENV{PARMETISDIR} PATH_SUFFIXES include)
  find_library(PARMETIS_LIBRARY parmetis PATHS $ENV{PARMETISDIR} PATH_SUFFIXES lib)
  set(PARMETIS_LIBRARIES ${PARMETIS_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PARMETIS DEFAULT_MSG PARMETIS_INCLUDES PARMETIS_LIBRARIES)
