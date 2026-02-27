cmake_minimum_required(VERSION 3.28)

project(jq C CXX)

option( BUILD_JQPP "Build jq c++ library" ON )
option( BUILD_CLI "Build jq_cli command" ON )
option( BUILD_JQPPTEST "Build jq c++ test command" ON )
option( BUILD_WITH_DECNUMBER "Build with decnumber" OFF )
option( BUILD_WITH_ONIG "Build with oniguruma" ON )
option( BUILD_ONIG_SHARED "Build onig as shared lib " OFF )
option( BUILD_WITHOUT_CONFIG "Build withoug config_opts" ON )

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
# Setup paths
#

set(JQ_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(JQ_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
set( DECNUMBER_PATH "${JQ_VENDOR_DIR}/decNumber" )
set( ONIGURUMA_PATH "${JQ_VENDOR_DIR}/oniguruma/src" )

if( WIN32 )
    # as installed by winget install "WinFlexBison.win_flex_bison_Microsoft"
    set( BISONFLEX_PATH "C:/Users/wimde/AppData/Local/Microsoft/WinGet/Packages/WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe" )
endif()

#
# generate config.h
#

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckStructHasMember)

# --- 1. Math Library Detection ---
if(UNIX)
    find_library(M_LIB m)
endif()

# Define a helper to check math functions specifically
macro(check_math_func FUNC)
    string(TOUPPER ${FUNC} FUNC_UPPER)
    set(CMAKE_REQUIRED_LIBRARIES ${M_LIB})
    check_symbol_exists(${FUNC} "math.h" HAVE_${FUNC_UPPER})
    set(CMAKE_REQUIRED_LIBRARIES)
endmacro()

# --- 2. Check Math Functions ---
set(MATH_FUNCS
    acos acosh asin asinh atan atan2 atanh cbrt ceil copysign cos cosh
    drem erf erfc exp exp10 exp2 expm1 fabs fdim floor fma fmax fmin
    fmod frexp gamma hypot j0 j1 jn ldexp lgamma lgamma_r log log10
    log1p log2 logb modf nearbyint nextafter nexttoward pow remainder
    rint round scalb scalbln sin sinh sqrt tan tanh tgamma trunc y0 y1 yn __exp10
)

foreach(f ${MATH_FUNCS})
    check_math_func(${f})
endforeach()

# --- 3. Check General POSIX / Standard Functions ---
# Note: On Windows, some are prefixed with underscore (e.g. _isatty)
set(STD_FUNCS atexit gettimeofday gmtime gmtime_r isatty localtime_r
    memmem setlocale strftime strptime timegm _isatty)

set( INCLUDES_TO_CHECK "stdlib.h;time.h;string.h;locale.h")
if( WIN32 )
    list( APPEND INCLUDES_TO_CHECK "io.h")
else()
    list( APPEND INCLUDES_TO_CHECK "unistd.h")
endif()

foreach(f ${STD_FUNCS})
    string(TOUPPER ${f} FUNC_UPPER)
    # Check common headers for these symbols
    set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")
    check_function_exists(${f} HAVE_${FUNC_UPPER})
    if( NOT HAVE_${FUNC_UPPER} )
        check_symbol_exists(${f} "${INCLUDES_TO_CHECK}" HAVE_${FUNC_UPPER})
    endif()
    set(CMAKE_REQUIRED_DEFINITIONS)
endforeach()

if( UNIX )
    check_include_files("pthread.h" HAVE_PTHREAD )
    if( HAVE_PTHREAD )
        set(CMAKE_REQUIRED_LIBRARIES pthread)
        check_symbol_exists(pthread_key_create "pthread.h" HAVE_PTHREAD_KEY_CREATE)
        check_symbol_exists(pthread_once "pthread.h" HAVE_PTHREAD_ONCE)
        set(CMAKE_REQUIRED_LIBRARIES)
    endif()
else()
    set( HAVE_PTHREAD 0 )
endif()

# --- 5. Check Headers ---
if( WIN32 )
    check_symbol_exists(alloca "malloc.h" HAVE_ALLOCA_H)
else()
    check_include_files("alloca.h" HAVE_ALLOCA_H)
endif()

# --- 6. Check Struct Members (Timezone offsets) ---
# Some systems use tm_gmtoff, others use __tm_gmtoff
check_struct_has_member("struct tm" tm_gmtoff "time.h" HAVE_TM_TM_GMT_OFF)
check_struct_has_member("struct tm" __tm_gmtoff "time.h" HAVE_TM___TM_GMT_OFF)

if( BUILD_WITH_ONIG )
    list( APPEND CMAKE_REQUIRED_INCLUDES "${ONIGURUMA_PATH}" )
    check_include_files("oniguruma.h" HAVE_LIBONIG)
endif()

# --- 7. Generate config.h ---
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/jq_config.h
)

#
# generate builtin.inc
#

# Read the binary file (e.g., builtin.jq) into a hex string
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/builtin.jq" HEX_CONTENTS LIMIT 1000000 HEX)

# Use regex to add '0x' prefix and commas (e.g., '6162' -> '0x61, 0x62')
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " C_ARRAY_CONTENTS "${HEX_CONTENTS}")

# Write it out to a header file
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/builtin.inc" "${C_ARRAY_CONTENTS}")

set_source_files_properties(builtin.c PROPERTIES OBJECT_DEPENDS "${BUILTIN_INC}")

#
# disable requirement for config_opts.inc
#

if( BUILD_WITHOUT_CONFIG )
	list( APPEND EXTRA_DEFINES NOCONFIG)
endif()

#
# detect version
#

# Find Git
find_package(Git REQUIRED)

# Run Git Describe to get the version string (e.g., v1.7.0-64-g1234abc)
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback if Git fails (e.g., if building from a zip without .git)
if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown-version")
endif()

if(GIT_VERSION MATCHES "^jq-")
    string(REGEX REPLACE "^jq-" "" GIT_VERSION "${GIT_VERSION}")
endif()

# Create the version.h from the template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

#
# detect endianity
#

if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
	list( APPEND EXTRA_DEFINES IEEE_MC68K )
else()
	list( APPEND EXTRA_DEFINES IEEE_8087 )
endif()

list( APPEND EXTRA_DEFINES _GNU_SOURCE )

#
# get BISON FLEX
#

if(WIN32)
	set(FLEX_EXECUTABLE "${BISONFLEX_PATH}/win_flex.exe")
	set(BISON_EXECUTABLE "${BISONFLEX_PATH}/win_bison.exe")
endif()

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

if(WIN32)
	# tell flex NOT to include unistd.h include
    set(FLEX_OPTS COMPILE_FLAGS "--nounistd" )
else()
	# Find the math library (this is the most portable way)
	find_library(MATH_LIBRARY m)
endif()

# Proceed with the targets as usual
BISON_TARGET(MyParser src/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)
FLEX_TARGET(MyLexer src/lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.c  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lexer.h ${FLEX_OPTS})

# Tell Flex to use the header generated by Bison
ADD_FLEX_BISON_DEPENDENCY(MyLexer MyParser)

include_directories("${JQ_SRC_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

if( HAVE_LIBONIG )
    include_directories("${JQ_SRC_DIR}" "${ONIGURUMA_PATH}" )
    # oniguruma dependency
    add_subdirectory(vendor/oniguruma ${CMAKE_CURRENT_BINARY_DIR}/oniguruma_build)
    if( NOT BUILD_ONIG_SHARED ) 
	    set_target_properties(onig PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

if( BUILD_WITH_DECNUMBER )
    include_directories("${JQ_SRC_DIR}" "${JQ_VENDOR_DIR}")
    file(GLOB JQ_DECNUMBER_SOURCES "${DECNUMBER_PATH}/*.c")
    file(GLOB JQ_DECNUMBER_HEADERS "${DECNUMBER_PATH}/*.h")
    list( APPEND EXTRA_DEFINES USE_DECNUM)
endif()

# Collect the source files (Excluding main.c so we build a library)
file(GLOB JQ_LIB_SOURCES "${JQ_SRC_DIR}/*.c")
file(GLOB JQ_LIB_HEADERS "${JQ_SRC_DIR}/*.h")
list(REMOVE_ITEM JQ_LIB_SOURCES "${JQ_SRC_DIR}/main.c" "${JQ_SRC_DIR}/parser.c" "${JQ_SRC_DIR}/lexer.c")
list(REMOVE_ITEM JQ_LIB_HEADERS "${JQ_SRC_DIR}/jqpp.h" "${JQ_SRC_DIR}/parser.h" "${JQ_SRC_DIR}/lexer.h")

#
# jq library
#

add_library(jq SHARED ${JQ_LIB_SOURCES} 
	${JQ_DECNUMBER_SOURCES} 
	${BISON_MyParser_OUTPUTS}
    	${FLEX_MyLexer_OUTPUTS} )
target_sources(jq PRIVATE ${JQ_LIB_HEADERS} 
	${JQ_DECNUMBER_HEADERS} 
	"${JQ_SRC_DIR}/parser.y" 
	"${JQ_SRC_DIR}/lexer.l" )

# Ensure the compiler can find the generated parser.h
target_include_directories(jq PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(MATH_LIBRARY)
    target_link_libraries(jq PRIVATE ${MATH_LIBRARY})
endif()

target_compile_definitions(jq PUBLIC ${EXTRA_DEFINES} )

if(WIN32)
    # Tell CMake to export all functions (Crucial for the .lib file)
    set_target_properties(jq PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

    # Add definitions for Windows compatibility
    target_compile_definitions(jq PRIVATE _CRT_SECURE_NO_WARNINGS HAVE_CONFIG_H=0 UNICODE _UNICODE)

    target_link_libraries(jq PRIVATE shlwapi)
endif()

if( HAVE_LIBONIG )
    target_link_libraries(jq PRIVATE onig)
    add_dependencies(jq onig)
    target_include_directories(jq PRIVATE "${JQ_SRC_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}" "${ONIGURUMA_PATH}" )
endif()

if( BUILD_CLI )
    # Build the CLI tool as well (Optional)
    add_executable(jq_cli "${JQ_SRC_DIR}/main.c")
    target_include_directories(jq_cli PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(jq_cli PRIVATE jq)
    if( HAVE_LIBONIG )
        target_link_libraries(jq_cli PRIVATE onig)
    endif()
    target_compile_definitions(jq_cli PUBLIC ${EXTRA_DEFINES} )
    add_dependencies( jq_cli jq )
endif()

#
# jqpp library
#

if( BUILD_JQPP )
    file(GLOB JQPP_LIB_SOURCES "${JQ_SRC_DIR}/*.cpp")
    list(REMOVE_ITEM JQPP_LIB_SOURCES "${JQ_SRC_DIR}/jqpp_main.cpp")

    add_library(jqpp SHARED ${JQ_DECNUMBER_SOURCES} 
	    ${JQ_LIB_SOURCES} 
	    ${JQPP_LIB_SOURCES} 
	    ${BISON_MyParser_OUTPUTS}
	    ${FLEX_MyLexer_OUTPUTS})
target_sources(jqpp PRIVATE ${JQPP_LIB_HEADERS} 
	${JQ_LIB_HEADERS} 
	${JQ_DECNUMBER_HEADERS} )
    target_compile_definitions(jqpp PUBLIC ${EXTRA_DEFINES} )
    set_target_properties(jqpp PROPERTIES LINKER_LANGUAGE CXX)
    # Ensure the compiler can find the generated parser.h
    target_include_directories(jqpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(MATH_LIBRARY)
    target_link_libraries(jqpp PRIVATE ${MATH_LIBRARY})
endif()

    if(WIN32)
        # Tell CMake to export all functions (Crucial for the .lib file)
        set_target_properties(jqpp PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
        target_link_libraries(jqpp PRIVATE shlwapi)
    endif()

    if( HAVE_LIBONIG )
        target_link_libraries(jqpp PRIVATE onig)
        add_dependencies(jqpp onig)
        target_include_directories(jqpp PRIVATE
            "${JQ_SRC_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" "${ONIGURUMA_PATH}")
    endif()

endif()

if( BUILD_JQPPTEST )
    add_executable(jqpp_test "${JQ_SRC_DIR}/jqpp_main.cpp")
    set_target_properties(jqpp_test PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(jqpp_test jqpp)
    target_compile_definitions(jqpp_test PUBLIC EXTRA_DEFINES )
    add_dependencies( jqpp_test jqpp )

endif()
