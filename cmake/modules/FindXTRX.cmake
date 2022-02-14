#
# Copyright 2013-2021 Software Radio Systems Limited
#
# This file is part of srsRAN
#
# srsRAN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsRAN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#

INCLUDE(FindPkgConfig)
#PKG_CHECK_MODULES(XTRX XTRX)
IF(NOT XTRX_FOUND)

FIND_PATH(
    XTRX_INCLUDE_DIRS
    NAMES xtrx_api.h
    HINTS $ENV{XTRX_DIR}/libxtrx
    PATHS /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    XTRX_LIBRARY
    NAMES xtrx
    HINTS $ENV{XTRX_DIR}/build/libxtrx
    PATHS /usr/local/lib
          /usr/lib
          /usr/lib/x86_64-linux-gnu
          /usr/local/lib64
          /usr/local/lib32
)

set(XTRX_LIBRARIES ${XTRX_LIBRARY})

message(STATUS "XTRX LIBRARIES " ${XTRX_LIBRARIES})
message(STATUS "XTRX INCLUDE DIRS " ${XTRX_INCLUDE_DIRS})

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XTRX DEFAULT_MSG XTRX_LIBRARIES XTRX_INCLUDE_DIRS)
MARK_AS_ADVANCED(XTRX_LIBRARIES XTRX_INCLUDE_DIRS)

ENDIF(NOT XTRX_FOUND)
