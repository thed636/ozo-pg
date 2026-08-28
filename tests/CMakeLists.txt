find_program(CCACHE_FOUND ccache)

include(ExternalProject)
ExternalProject_Add(
    GoogleTest
    GIT_REPOSITORY "https://github.com/google/googletest.git"
    GIT_TAG v1.15.2
    # GoogleTest must be built with the same C++ standard as the tests that
    # consume it: its std::string_view matcher specializations are only
    # compiled under C++17 and later, and linking a build at a different
    # standard against these test objects fails with undefined
    # Matcher<string_view> symbols.
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
    UPDATE_COMMAND ""
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
)
ExternalProject_Get_Property(GoogleTest source_dir)
include_directories(SYSTEM "${source_dir}/include")
include_directories(SYSTEM "${CMAKE_CURRENT_BINARY_DIR}/include")
link_directories("${CMAKE_CURRENT_BINARY_DIR}/lib")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

set(SOURCES
    impl/async_connect.cpp
    binary_deserialization.cpp
    binary_query.cpp
    binary_serialization.cpp
    bind.cpp
    composite.cpp
    connection.cpp
    connection_info.cpp
    connection_pool.cpp
    query_builder.cpp
    query_conf.cpp
    type_traits.cpp
    concept.cpp
    result.cpp
    none.cpp
    deadline.cpp
    error.cpp
    impl/async_send_query_params.cpp
    impl/async_get_result.cpp
    detail/base36.cpp
    detail/begin_statement_builder.cpp
    detail/functional.cpp
    detail/timeout_handler.cpp
    detail/make_copyable.cpp
    impl/request_oid_map.cpp
    impl/request_oid_map_handler.cpp
    impl/async_start_transaction.cpp
    impl/async_end_transaction.cpp
    transaction_status.cpp
    impl/async_request.cpp
    io/size_of.cpp
    failover/retry.cpp
    failover/strategy.cpp
    failover/role_based.cpp
    detail/deadline.cpp
    impl/cancel.cpp
    transaction.cpp
    main.cpp
)

if(OZO_BUILD_PG_TESTS)
    set(SOURCES
        ${SOURCES}
        integration/result_integration.cpp
        integration/request_integration.cpp
        integration/get_connection_integration.cpp
        integration/execute_integration.cpp
        integration/transaction_integration.cpp
        integration/retry_integration.cpp
        integration/cancel_integration.cpp
        integration/role_based_integration.cpp
        integration/connection_pool_integration.cpp
        integration/executor_integration.cpp
    )
    add_definitions(-DOZO_PG_TEST_CONNINFO="${OZO_PG_TEST_CONNINFO}")
endif()

add_executable(ozo_tests ${SOURCES})
add_dependencies(ozo_tests GoogleTest)
target_link_libraries(ozo_tests gtest)
target_link_libraries(ozo_tests gmock)
target_link_libraries(ozo_tests ozo)
add_test(ozo_tests ozo_tests)

if(CCACHE_FOUND)
    set_target_properties(ozo_tests PROPERTIES RULE_LAUNCH_COMPILE ccache)
    set_target_properties(ozo_tests PROPERTIES RULE_LAUNCH_LINK ccache)
endif()

# enable useful warnings and errors
target_compile_options(ozo_tests PRIVATE -Wall -Wextra -pedantic -Werror)

# ignore specific errors for clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(ozo_tests PRIVATE -Wno-ignored-optimization-argument)
endif()

# Coroutine tests live in their own target purely to keep the completion-token
# cases together; they need a database, since the awaitable token is only
# meaningfully exercised against real IO.
if(OZO_BUILD_PG_TESTS)
    add_executable(ozo_coroutine_tests integration/coroutine_integration.cpp main.cpp)
    add_dependencies(ozo_coroutine_tests GoogleTest)
    target_link_libraries(ozo_coroutine_tests gtest)
    target_link_libraries(ozo_coroutine_tests gmock)
    target_link_libraries(ozo_coroutine_tests ozo)
    add_test(ozo_coroutine_tests ozo_coroutine_tests)

    if(CCACHE_FOUND)
        set_target_properties(ozo_coroutine_tests PROPERTIES RULE_LAUNCH_COMPILE ccache)
        set_target_properties(ozo_coroutine_tests PROPERTIES RULE_LAUNCH_LINK ccache)
    endif()

    target_compile_options(ozo_coroutine_tests PRIVATE -Wall -Wextra -pedantic -Werror)

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        target_compile_options(ozo_coroutine_tests PRIVATE -Wno-ignored-optimization-argument)
    endif()

    # See the same suppression in examples/CMakeLists.txt: GCC reports spurious
    # -Wmaybe-uninitialized inside coroutine frames in optimized builds. The
    # tests are normally built unoptimized, where it does not fire, but this
    # keeps a release-mode test build working too.
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(ozo_coroutine_tests PRIVATE -Wno-maybe-uninitialized)
    endif()
endif()

if(OZO_COVERAGE AND CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    set(COVERAGE_EXCLUDES "'.*/(tests|contrib|gmock|gtest)/.*'")
    SETUP_TARGET_FOR_COVERAGE_GCOVR(
        NAME ozo_coverage
        EXECUTABLE ctest -V
        DEPENDENCIES ozo_tests
    )
endif()

# resource_pool's own test suite, vendored alongside its headers.
#
# OZO's integration tests exercise the pool only indirectly, through three
# connection_pool cases. These are its unit tests: the wait queue with
# per-request timeouts, idle and lifespan expiry, and the handle's recycle and
# waste semantics. They are the safety net for any change to the pool, which is
# concurrent code where the failure modes -- timer races against acquisition,
# cancellation mid-wait, use after recycle -- do not reproduce reliably from an
# integration test.
#
# The synchronous pool's tests are deliberately absent: that pool is not
# vendored, since nothing in OZO reaches it.
if(OZO_BUILD_TESTS)
    set(RESOURCE_POOL_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests/async/integration.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests/async/pool.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests/async/pool_impl.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests/async/queue.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests/error.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests/handle.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests/time_traits.cc
        main.cpp
    )

    add_executable(ozo_resource_pool_tests ${RESOURCE_POOL_TEST_SOURCES})
    add_dependencies(ozo_resource_pool_tests GoogleTest)
    target_link_libraries(ozo_resource_pool_tests gtest gmock ozo Boost::coroutine Boost::context)
    target_include_directories(ozo_resource_pool_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../contrib/resource_pool/tests)
    add_test(ozo_resource_pool_tests ozo_resource_pool_tests)

    if(CCACHE_FOUND)
        set_target_properties(ozo_resource_pool_tests PROPERTIES RULE_LAUNCH_COMPILE ccache)
        set_target_properties(ozo_resource_pool_tests PROPERTIES RULE_LAUNCH_LINK ccache)
    endif()

    target_compile_options(ozo_resource_pool_tests PRIVATE -Wall -Wextra -pedantic -Werror)

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        target_compile_options(ozo_resource_pool_tests PRIVATE -Wno-ignored-optimization-argument)
    endif()
endif()
