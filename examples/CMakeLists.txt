find_program(CCACHE_FOUND ccache)

if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

add_executable(ozo_request request.cpp)
target_link_libraries(ozo_request ozo)

# enable a bunch of warnings and make them errors
target_compile_options(ozo_request PRIVATE -Wall -Wextra -Wsign-compare -pedantic -Werror)

# ignore specific errors for clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(ozo_request PRIVATE -Wno-ignored-optimization-argument)
endif()

add_executable(ozo_transaction transaction.cpp)
target_link_libraries(ozo_transaction ozo)

# enable a bunch of warnings and make them errors
target_compile_options(ozo_transaction PRIVATE -Wall -Wextra -Wsign-compare -pedantic -Werror)

# ignore specific errors for clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(ozo_transaction PRIVATE -Wno-ignored-optimization-argument)
endif()

add_executable(ozo_retry_request retry_request.cpp)
target_link_libraries(ozo_retry_request ozo)

# enable a bunch of warnings and make them errors
target_compile_options(ozo_retry_request PRIVATE -Wall -Wextra -Wsign-compare -pedantic -Werror)

# ignore specific errors for clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(ozo_retry_request PRIVATE -Wno-ignored-optimization-argument)
endif()

add_executable(ozo_role_based_request role_based_request.cpp)
target_link_libraries(ozo_role_based_request ozo)

# enable a bunch of warnings and make them errors
target_compile_options(ozo_role_based_request PRIVATE -Wall -Wextra -Wsign-compare -pedantic -Werror)

# ignore specific errors for clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(ozo_role_based_request PRIVATE -Wno-ignored-optimization-argument)
endif()

add_executable(ozo_connection_pool connection_pool.cpp)
target_link_libraries(ozo_connection_pool ozo)

# enable a bunch of warnings and make them errors
target_compile_options(ozo_connection_pool PRIVATE -Wall -Wextra -Wsign-compare -pedantic -Werror)

# ignore specific errors for clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(ozo_connection_pool PRIVATE -Wno-ignored-optimization-argument)
endif()

add_executable(ozo_coroutine coroutine.cpp)
target_link_libraries(ozo_coroutine ozo)

# enable a bunch of warnings and make them errors
target_compile_options(ozo_coroutine PRIVATE -Wall -Wextra -Wsign-compare -pedantic -Werror)

# ignore specific errors for clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(ozo_coroutine PRIVATE -Wno-ignored-optimization-argument)
endif()

# GCC reports spurious -Wmaybe-uninitialized inside coroutine frames once
# optimization is enabled: the diagnostic points at the closing brace of the
# coroutine rather than at any variable. It is a known GCC limitation in
# analysing coroutine frames, not a defect in this example, and it does not
# appear in unoptimized builds.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(ozo_coroutine PRIVATE -Wno-maybe-uninitialized)
endif()
