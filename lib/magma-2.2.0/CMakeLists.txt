cmake_minimum_required( VERSION 2.8.1 )

# ----------------------------------------
# to disable Fortran, set this to "off"
# see also -DADD_ below
set( USE_FORTRAN on CACHE BOOL "Fortran is required for some tester checks, but can be disabled with reduced functionality" )

if ( USE_FORTRAN )
	project( MAGMA C CXX Fortran )
else()
	project( MAGMA C CXX )
endif()


# ----------------------------------------
# to show compile commands, set this here or use 'make VERBOSE=1'
#set(CMAKE_VERBOSE_MAKEFILE on)


# ----------------------------------------
# don't regenerate files during make.
# (I think this means you have to manually re-run CMake if CMakeLists changes.
# It fixes the huge problems with CMake interrupting Visual Studio.)
set(CMAKE_SUPPRESS_REGENERATION on)


# ----------------------------------------
# force an out-of-source build, to not overwrite the existing Makefiles
# (out-of-source is cleaner, too)
string( COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" MAGMA_COMPILE_INPLACE )
if( MAGMA_COMPILE_INPLACE )
	message( FATAL_ERROR "Compiling MAGMA with CMake requires an out-of-source build. To proceed:
    rm -rf CMakeCache.txt CMakeFiles/   # delete files in ${CMAKE_SOURCE_DIR}
    mkdir build
    cd build
    cmake ..
    make" )
endif()


# ----------------------------------------
# use C++11 and C99
# see http://stackoverflow.com/questions/10851247/how-to-activate-c-11-in-cmake
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
	message( WARNING "The compiler ${CMAKE_CXX_COMPILER} doesn't support the -std=c++11 flag. Some code may not compile.")
endif()

CHECK_C_COMPILER_FLAG("-std=c99" COMPILER_SUPPORTS_C99)
if(COMPILER_SUPPORTS_C99)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
else()
	message( WARNING "The compiler ${CMAKE_C_COMPILER} doesn't support the -std=c99 flag. Some code may not compile.")
endif()


# ----------------------------------------
# check Fortran name mangling
if ( USE_FORTRAN )
	include( FortranCInterface )
	FortranCInterface_HEADER( ${CMAKE_SOURCE_DIR}/include/magma_mangling_cmake.h MACRO_NAMESPACE MAGMA_ )
else()
	# set one of -DADD_, -DUPCASE, or -DNOCHANGE. See README.
	message( STATUS "Building without Fortran compiler" )
	set( FORTRAN_CONVENTION "-DADD_" CACHE STRING "Fortran calling convention, one of -DADD_, -DNOCHANGE, -DUPCASE" )
	message( STATUS "    Using ${FORTRAN_CONVENTION} for Fortran calling convention" )
	set( CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${FORTRAN_CONVENTION}" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FORTRAN_CONVENTION}" )
	# see also NVCC_FLAGS below
endif()


# ----------------------------------------
# locate OpenMP
find_package( OpenMP )
if ( OPENMP_FOUND )
	message( STATUS "Found OpenMP" )
	message( STATUS "    OpenMP_C_FLAGS   ${OpenMP_C_FLAGS}" )
	message( STATUS "    OpenMP_CXX_FLAGS ${OpenMP_CXX_FLAGS}" )
	set( CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}" )
endif()


# ----------------------------------------
# locate CUDA libraries
set( GPU_TARGET "Fermi Kepler" CACHE STRING "CUDA architectures to compile for; one or more of Fermi, Kepler, or Maxwell" )
find_package( CUDA )
if ( CUDA_FOUND )
	message( STATUS "Found CUDA ${CUDA_VERSION}" )
	message( STATUS "    CUDA_INCLUDE_DIRS:   ${CUDA_INCLUDE_DIRS}"   )
	message( STATUS "    CUDA_CUDART_LIBRARY: ${CUDA_CUDART_LIBRARY}" )
	#message( STATUS "    CUDA_LIBRARIES: ${CUDA_LIBRARIES}" )
	#message( STATUS "    CUDA_CUBLAS_LIBRARIES: ${CUDA_CUBLAS_LIBRARIES}" )
	include_directories( ${CUDA_INCLUDE_DIRS} )

	# NVCC options for the different cards
	# sm_xx is binary, compute_xx is PTX for forward compatability
	# MIN_ARCH is lowest requested version 
	# NV_SM    accumulates sm_xx for all requested versions
	# NV_COMP  is compute_xx for highest requested version
	set( NV_SM    "" )
	set( NV_COMP  "" )

	# -std=c++11 messes with nvcc, so don't propagate CXXFLAGS to nvcc.
	# BUT, Windows requires flags to be propagated to get /MD correct.
	# Since Visual Studio doesn't use -std, test that for now.
	# Ultimately, this is a bug in CMake's FindCUDA module:
	# https://cmake.org/Bug/view.php?id=13450
	if ( ${CMAKE_CXX_FLAGS} MATCHES -std= )
		set( CUDA_PROPAGATE_HOST_FLAGS OFF )
	endif()
	
	if ( ${GPU_TARGET} MATCHES Fermi )
		set( GPU_TARGET "${GPU_TARGET} sm20" )
	endif()
	
	if ( ${GPU_TARGET} MATCHES Kepler )
		set( GPU_TARGET "${GPU_TARGET} sm30 sm35" )
	endif()
	
	if ( ${GPU_TARGET} MATCHES Maxwell )
		set( GPU_TARGET "${GPU_TARGET} sm50" )
	endif()
	
	if ( ${GPU_TARGET} MATCHES sm20 )
		if ( NOT MIN_ARCH )
			set( MIN_ARCH 200 )
		endif()
		set( NV_SM "${NV_SM} -gencode arch=compute_20,code=sm_20" )
		set( NV_COMP        "-gencode arch=compute_20,code=compute_20" )
		message( STATUS "    compile for CUDA arch 2.x (Fermi)" )
	endif()
	
	if ( ${GPU_TARGET} MATCHES sm30 )
		if ( NOT MIN_ARCH )
			set( MIN_ARCH 300 )
		endif()
		set( NV_SM "${NV_SM} -gencode arch=compute_30,code=sm_30" )
		set( NV_COMP        "-gencode arch=compute_30,code=compute_30" )
		message( STATUS "    compile for CUDA arch 3.0 (Kepler)" )
	endif()
	
	if ( ${GPU_TARGET} MATCHES sm35 )
		if ( NOT MIN_ARCH )
			set( MIN_ARCH 300 )
		endif()
		set( NV_SM "${NV_SM} -gencode arch=compute_35,code=sm_35" )
		set( NV_COMP        "-gencode arch=compute_35,code=compute_35" )
		message( STATUS "    compile for CUDA arch 3.5 (Kepler)" )
	endif()
	
	if ( ${GPU_TARGET} MATCHES sm50 )
		if ( NOT MIN_ARCH )
			set( MIN_ARCH 500 )
		endif()
		set( NV_SM "${NV_SM} -gencode arch=compute_50,code=sm_50" )
		set( NV_COMP        "-gencode arch=compute_50,code=compute_50" )
		message( STATUS "    compile for CUDA arch 5.0 (Maxwell)" )
	endif()
	
	if ( NOT MIN_ARCH )
		message( FATAL_ERROR "GPU_TARGET must contain one or more of Fermi, Kepler, Maxwell, or sm{20,30,35,50}." )
	endif()
	
	set( CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -DHAVE_CUBLAS ${NV_SM} ${NV_COMP} ${FORTRAN_CONVENTION}" )
	add_definitions( "-DHAVE_CUBLAS -DMIN_CUDA_ARCH=${MIN_ARCH}" )
	message( STATUS "Define -DHAVE_CUBLAS -DMIN_CUDA_ARCH=${MIN_ARCH}" )
endif()


# ----------------------------------------
# locate LAPACK libraries
set( LAPACK_LIBRARIES "" CACHE STRING "Libraries for LAPACK and BLAS, to manually override search" )
if ( "${LAPACK_LIBRARIES}" STREQUAL "" )
	message( STATUS "Searching for BLAS and LAPACK. To override, set LAPACK_LIBRARIES using ccmake." )
	find_package( LAPACK )
else()
	message( STATUS "User set LAPACK_LIBRARIES. To change, edit LAPACK_LIBRARIES using ccmake (set to empty to enable search)." )
	# Check existence -- but this may be okay, if the user entered, e.g., -llapack instead of /path/to/lapack.a
	foreach( LIB ${LAPACK_LIBRARIES} )
		if ( NOT EXISTS ${LIB} )
			message( WARNING "\n      Warning: file ${LIB} does not exist.\n" )
		endif()
	endforeach()
endif()
message( STATUS "    BLAS_LIBRARIES:      ${BLAS_LIBRARIES}"      )
message( STATUS "    LAPACK_LIBRARIES:    ${LAPACK_LIBRARIES}"    )

# If using MKL, add it to includes and define MAGMA_WITH_MKL
# Initially, this gets MKLROOT from environment, but then the user can edit it.
set( MKLROOT $ENV{MKLROOT} CACHE STRING "MKL installation directory" )
if ( NOT "${MKLROOT}" STREQUAL "" )
	message( STATUS "MKLROOT set to ${MKLROOT}. To change, edit MKLROOT using ccmake." )
	if ( NOT LAPACK_LIBRARIES MATCHES mkl_core )
		message( WARNING "MKLROOT set, but LAPACK_LIBRARIES doesn't contain mkl_core; ignoring MKLROOT." )
	else()
		if ( NOT EXISTS "${MKLROOT}" )
			message( FATAL_ERROR "MKLROOT ${MKLROOT} directory does not exist." )
		endif()
		include_directories( ${MKLROOT}/include )
		add_definitions( -DMAGMA_WITH_MKL )
		message( STATUS "Define -DMAGMA_WITH_MKL" )
	endif()
else()
	message( STATUS "MKLROOT not set. To change, set MKLROOT using ccmake." )
endif()


# ----------------------------------------
# save magma.lib, magma_sparse.lib, etc. in lib/
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY lib )


# ----------------------------------------
# list of sources
include( ${CMAKE_SOURCE_DIR}/CMake.src )


# ----------------------------------------
# common flags

set( LIBS tester lapacktest magma )

if ( WIN32 )
	# On Windows:
	#     Strip out /W3; we will use -W4
	#     -Wall is way too verbose; use -W4
	#     -MP enables parallel builds
	#     -std=c99 is not implemented, so skip that
	string( REGEX REPLACE " */W3" "" CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}"   )
	string( REGEX REPLACE " */W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
	set( CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -W4 -MP -DMAGMA_NOAFFINITY" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W4 -MP -DMAGMA_NOAFFINITY" )
else()
	# Primarily for gcc / nvcc:
	# Ignore unused static functions in headers.
	set( CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -Wall -Wno-unused-function" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-function" )
endif()

if ( CMAKE_HOST_APPLE )
	# 64-bit veclib (Accelerate) has issues; substitute correct functions from LAPACK.
	# (The issue is single precision functions that return doubles;
	# if a consistent prototype is used, the problem goes away in C,
	# but this is not feasible in Fortran.)
	if ( "${LAPACK_LIBRARIES}" MATCHES "Accelerate" )
		if ( USE_FORTRAN )
			message( STATUS "MacOS X: adding blas_fix library" )
			add_library( blas_fix ${libblas_fix_src} )
			list( APPEND LIBS blas_fix )
			set( blas_fix blas_fix )
		else()
			message( WARNING "\n      Warning: cannot compile blas_fix library for MacOS X without Fortran compiler.\n" )
		endif()
	endif()
	
	set( CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -DMAGMA_NOAFFINITY" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMAGMA_NOAFFINITY" )
	
	# previously, just compile as 32-bit, but CUDA 6.5 no longer has 32-bit FAT libraries
	## set( CMAKE_C_FLAGS       "${CMAKE_C_FLAGS} -m32" )
	## set( CMAKE_CXX_FLAGS     "${CMAKE_CXX_FLAGS} -m32" )
	## set( CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -m32" )
	## set( CUDA_64_BIT_DEVICE_CODE OFF )
endif()

include_directories( include )
include_directories( control )

# Need to check sizeof(void*) after setting flags above;
# CMAKE_SIZEOF_VOID_P can be wrong.
include( CheckTypeSize )
CHECK_TYPE_SIZE( void* SIZEOF_VOID_PTR )
if ( USE_FORTRAN )
	set( CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -Dmagma_devptr_t=\"integer\(kind=${SIZEOF_VOID_PTR}\)\"" )
endif()


# ----------------------------------------
# compile MAGMA library
if ( WIN32 )
	# Windows seems to have a problem mixing C, CUDA, and Fortran files
	# Currently ignores .f90 and .F90 files, because it doesn't seem to
	# understand that .F90 files should be pre-processed.
	
	# separate Fortran and C/C++/CUDA files
	foreach( f ${libmagma_all} )
		if ( ${f} MATCHES "\\.(f)$" )  # |f90|F90
			list( APPEND libmagma_all_f   ${f} )
		elseif ( ${f} MATCHES "\\.(c|cu|cpp)$" )
			list( APPEND libmagma_all_cpp ${f} )
		endif()
	endforeach()
	#message( "libmagma_all_cpp ${libmagma_all_cpp}" )
	#message( "libmagma_all_f   ${libmagma_all_f}"   )
	
	# on Windows, Fortran files aren't compiled if listed here...
	cuda_add_library( magma ${libmagma_all_cpp} )
	target_link_libraries( magma
		${LAPACK_LIBRARIES}
		${CUDA_CUDART_LIBRARY}
		${CUDA_CUBLAS_LIBRARIES}
		${CUDA_cusparse_LIBRARY}
	)
	
	# no Fortran files at the moment (how to test libmagma_all_f is not empty?),
	# but keep this around for future reference.
	#
	##  ...so make a separate library out of Fortran files. Ugh.
	## add_library( magmaf ${libmagma_all_f} )
	## target_link_libraries( magmaf
	##     ${LAPACK_LIBRARIES}
	##     ${CUDA_CUDART_LIBRARY}
	##     ${CUDA_CUBLAS_LIBRARIES}
	##     ${CUDA_cusparse_LIBRARY}
	## )
	
	set( LIBS tester lapacktest magma )  ##magmaf )
else()
	# Unix doesn't seem to have a problem with mixing C, CUDA, and Fortran files
	cuda_add_library( magma ${libmagma_all} )
	target_link_libraries( magma
		${LAPACK_LIBRARIES}
		${CUDA_CUDART_LIBRARY}
		${CUDA_CUBLAS_LIBRARIES}
		${CUDA_cusparse_LIBRARY}
	)
endif()


# ----------------------------------------
# compile tester library
add_library( tester ${libtest_all} )


# ----------------------------------------
# compile lapacktest library
# If use fortran, compile only Fortran files, not magma_[sdcz]_no_fortran.cpp
# else,           compile only C++     files, not Fortran files
if ( USE_FORTRAN )
	foreach( f ${liblapacktest_all} )
		if ( ${f} MATCHES "\\.(f|f90)$" )
			list( APPEND liblapacktest_all_f ${f} )
		endif()
	endforeach()
	add_library( lapacktest ${liblapacktest_all_f} )
else()
	# alternatively, use only C/C++/CUDA files, including magma_[sdcz]_no_fortran.cpp
	foreach( f ${liblapacktest_all} )
		if ( ${f} MATCHES "\\.(c|cu|cpp)$" )
			list( APPEND liblapacktest_all_cpp ${f} )
		endif()
	endforeach()
	add_library( lapacktest ${liblapacktest_all_cpp} )
endif()


# ----------------------------------------
# compile MAGMA sparse library

# sparse doesn't have Fortran at the moment, so no need for above shenanigans
include_directories( sparse/include )
include_directories( sparse/control )
include_directories( testing )
cuda_add_library( magma_sparse ${libsparse_all} )
target_link_libraries( magma_sparse
	magma
	${LAPACK_LIBRARIES}
	${CUDA_CUDART_LIBRARY}
	${CUDA_CUBLAS_LIBRARIES}
	${CUDA_cusparse_LIBRARY}
)
set( LIBS_SPARSE ${LIBS} magma_sparse )


# ----------------------------------------
# compile each tester

# save testers to testing/
# save tester lib files to testing_lib/ to avoid cluttering lib/
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY testing )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY testing_lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY testing_lib )

# skip Fortran testers, which require an extra file from CUDA
foreach( f ${testing_all} )
	if ( ${f} MATCHES "\\.(c|cu|cpp)$" )
		list( APPEND testing_all_cpp ${f} )
	endif()
endforeach()
foreach( TEST ${testing_all_cpp} )
	string( REGEX REPLACE "\\.(cpp|f90|F90)" "" EXE ${TEST} )
	string( REGEX REPLACE "testing/" "" EXE ${EXE} )
	#message( "${TEST} --> ${EXE}" )
	add_executable( ${EXE} ${TEST} )
	target_link_libraries( ${EXE} ${LIBS} )
endforeach()


# ----------------------------------------
# compile each sparse tester
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY sparse/testing )
foreach( TEST ${sparse_testing_all} )
	string( REGEX REPLACE "\\.(cpp|f90|F90)"     "" EXE ${TEST} )
	string( REGEX REPLACE "sparse/testing/" "" EXE ${EXE} )
	#message( "${TEST} --> ${EXE}" )
	add_executable( ${EXE} ${TEST} )
	target_link_libraries( ${EXE} ${LIBS_SPARSE} )
endforeach()


# ----------------------------------------
# what to install
install( TARGETS magma magma_sparse ${blas_fix}
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib )
file( GLOB headers include/*.h sparse/include/*.h )
install( FILES ${headers}
         DESTINATION include )

# ----------------------------------------
get_directory_property( compile_definitions COMPILE_DEFINITIONS )

message( STATUS "Flags" )
message( STATUS "    CFLAGS       ${CMAKE_C_FLAGS}" )
message( STATUS "    CXXFLAGS     ${CMAKE_CXX_FLAGS}" )
message( STATUS "    NFLAGS       ${CUDA_NVCC_FLAGS}" )
message( STATUS "    FFLAGS       ${CMAKE_Fortran_FLAGS}" )
message( STATUS "    LIBS         ${LIBS}" )
message( STATUS "    LIBS_SPARSE  ${LIBS_SPARSE}" )
