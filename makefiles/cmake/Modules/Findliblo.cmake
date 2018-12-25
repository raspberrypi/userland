# - Try to find liblo
# Once done this will define
#
#  LIBLO_FOUND - system has liblo
#  LIBLO_INCLUDE_DIRS - the liblo include directory
#  LIBLO_LIBRARIES - Link these to use liblo
#  LIBLO_DEFINITIONS - Compiler switches required for using liblo
#
#  Adapted from cmake-modules Google Code project
#
#  Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#
#  (Changes for liblo) Copyright (c) 2008 Kyle Machulis <kyle@nonpolynomial.com>
#
# Redistribution and use is allowed according to the terms of the New BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LIBLO_LIBRARIES AND LIBLO_INCLUDE_DIRS)
  # in cache already
  set(LIBLO_FOUND TRUE)
else (LIBLO_LIBRARIES AND LIBLO_INCLUDE_DIRS)
  find_path(LIBLO_INCLUDE_DIR
    NAMES
      lo/lo.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(LIBLO_LIBRARY
    NAMES
      lo
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  set(LIBLO_INCLUDE_DIRS
    ${LIBLO_INCLUDE_DIR}
  )
  set(LIBLO_LIBRARIES
    ${LIBLO_LIBRARY}
)

  if (LIBLO_INCLUDE_DIRS AND LIBLO_LIBRARIES)
     set(LIBLO_FOUND TRUE)
  endif (LIBLO_INCLUDE_DIRS AND LIBLO_LIBRARIES)

  if (LIBLO_FOUND)
    if (NOT liblo_FIND_QUIETLY)
      message(STATUS "Found liblo: ${LIBLO_LIBRARIES}")
    endif (NOT liblo_FIND_QUIETLY)
  else (LIBLO_FOUND)
    if (liblo_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find liblo")
    endif (liblo_FIND_REQUIRED)
  endif (LIBLO_FOUND)

  # show the LIBLO_INCLUDE_DIRS and LIBLO_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBLO_INCLUDE_DIRS LIBLO_LIBRARIES)

endif (LIBLO_LIBRARIES AND LIBLO_INCLUDE_DIRS)
