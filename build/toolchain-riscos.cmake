# CMake toolchain file for cross-building YSCE for RISC OS with GCCSDK GCC 10
# (riscos-crossdev toolchain) and the riscos-mesa devkit.
#   cmake -DCMAKE_TOOLCHAIN_FILE=.../toolchain-riscos.cmake \
#         -DRISCOS_TOOLCHAIN=<toolchain dir> -DRISCOS_DEVKIT=<devkit dir> ../YSCE/src
# CMAKE_SYSTEM_NAME stays "Linux" so upstream's UNIX branches are kept where they
# suit us; every RISC OS difference is under if(RISCOS).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(RISCOS 1)

if(NOT RISCOS_TOOLCHAIN)
  set(RISCOS_TOOLCHAIN $ENV{RISCOS_TOOLCHAIN})
endif()
if(NOT RISCOS_DEVKIT)
  set(RISCOS_DEVKIT $ENV{RISCOS_DEVKIT})
endif()
set(RISCOS_TOOLCHAIN ${RISCOS_TOOLCHAIN} CACHE PATH "riscos-crossdev toolchain")
set(RISCOS_DEVKIT ${RISCOS_DEVKIT} CACHE PATH "riscos-mesa devkit")

set(CMAKE_C_COMPILER   ${RISCOS_TOOLCHAIN}/bin/arm-riscos-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${RISCOS_TOOLCHAIN}/bin/arm-riscos-gnueabihf-g++)
set(CMAKE_AR           ${RISCOS_TOOLCHAIN}/bin/arm-riscos-gnueabihf-ar CACHE FILEPATH "")
set(CMAKE_RANLIB       ${RISCOS_TOOLCHAIN}/bin/arm-riscos-gnueabihf-ranlib CACHE FILEPATH "")

# Same flags as the devkit examples. -fstack-clash-protection is essential.
# RISCOS_EXTRA_FLAGS: added by build.sh (-ffile-prefix-map). Don't pass
# CMAKE_C_FLAGS/CMAKE_CXX_FLAGS on the command line: they replace these.
# -fno-delete-null-pointer-checks: YSFlight's older classes (YsList and
# others, 32 places) call member functions on null pointers and test
# this==NULL inside. GCC 6 and later assume this is never null and delete
# those tests, so an empty list crashed (abort on data transfer at &004065C4
# in YsList<YsSwordPolygon>::Append, Pi, M2 test 1). This flag keeps them.
set(RISCOS_FLAGS "-mfpu=vfpv3 -mfloat-abi=hard -fstack-clash-protection -fno-delete-null-pointer-checks -I${RISCOS_DEVKIT}/include ${RISCOS_EXTRA_FLAGS}")
set(CMAKE_C_FLAGS_INIT   "${RISCOS_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${RISCOS_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -L${RISCOS_DEVKIT}/lib")

set(CMAKE_FIND_ROOT_PATH ${RISCOS_TOOLCHAIN}/arm-riscos-gnueabihf ${RISCOS_DEVKIT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# CMake's Linux platform adds -rdynamic to executables, which GCCSDK GCC
# rejects. The rules override (read after the platform files) clears it.
set(CMAKE_USER_MAKE_RULES_OVERRIDE ${CMAKE_CURRENT_LIST_DIR}/riscos-rules.cmake)
