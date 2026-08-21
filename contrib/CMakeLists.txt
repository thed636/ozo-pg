# resource_pool is vendored rather than consumed as a package.
#
# It is a small header-only library (a dozen headers) that OZO has always
# depended on, and it is no longer maintained upstream: the version on Conan
# Center is a 2021 snapshot pinned to Boost 1.79 which predates the port to
# asio::any_io_executor, so it does not build against the Boost this project
# requires. Carrying the headers here keeps OZO installable and packageable as
# a single self-contained unit, with no submodule to initialise and no
# unpublishable dependency in the way of releasing OZO itself.
#
# It is MIT licensed, copyright Roman Siromakha; see
# contrib/resource_pool/LICENSE, which is installed alongside OZO's own.

add_library(resource_pool INTERFACE)
add_library(elsid::resource_pool ALIAS resource_pool)

target_compile_features(resource_pool INTERFACE cxx_std_20)

target_include_directories(resource_pool INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resource_pool/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(resource_pool INTERFACE Boost::thread Boost::boost)

target_compile_definitions(resource_pool INTERFACE -DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

# Exported as part of the ozo target set rather than as a package of its own,
# so that find_package(ozo) needs no find_dependency(resource_pool).
install(
    TARGETS     resource_pool
    EXPORT      ozo-targets
    DESTINATION lib
)

install(
    DIRECTORY   resource_pool/include/yamail
    DESTINATION include
)

install(
    FILES       resource_pool/LICENSE
    DESTINATION share/licenses/ozo
    RENAME      LICENSE.resource_pool
)
