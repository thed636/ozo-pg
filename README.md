# OZO — header-only C++17 async PostgreSQL client built on Boost.Asio

[![CI](https://github.com/thed636/ozo-pg/actions/workflows/ci.yml/badge.svg)](https://github.com/thed636/ozo-pg/actions/workflows/ci.yml)

> **OZO** — from お象 (*o-zō*), Japanese for "the honorable elephant", after PostgreSQL's elephant.

## What's this

OZO is a C++17 library for asyncronous communication with PostgreSQL DBMS.
The library leverages the power of template metaprogramming, providing convenient mapping from C++ types to SQL along with rich query building possibilities. OZO supports different concurrency paradigms (callbacks, futures, coroutines), using Boost.Asio under the hood. Low-level communication with PostgreSQL server is done via libpq. All concepts in the library are designed to be easily extendable (even replaceable) by the user to simplify adaptation to specific project requirements.

### API

Since the project is on early state of development it lacks of documentation. We understand the importance of good docs and are working hard on this problem. Complete documentation is on the way, but now:

* look at our brand new [HOW TO](docs/howto.md),
* try our [generated from sources documentation](https://yandex.github.io/ozo/) - it is under construction but readable,
* learn more about main use-cases from [unit tests](tests/integration/request_integration.cpp),
* See our [C++Now'18 talk about OZO](https://youtu.be/-1zbaxuUsMA) with [presentation](https://github.com/boostcon/cppnow_presentations_2018/blob/master/05-09-2018_wednesday/design_and_implementation_of_dbms_asynchronous_client_library__roman_siromakha__cppnow_05092018.pdf).

### C++20 coroutines

Every OZO operation accepts an arbitrary Boost.Asio completion token, so C++20
coroutines work without the library needing to opt in — pass `asio::use_awaitable`
and `co_await` the result. The library itself remains C++17; only your own
translation units need C++20.

```cpp
// Prefer as_tuple: a bare use_awaitable reports failure by throwing, and the
// exception carries only an error_code. OZO keeps the useful diagnostics on the
// connection, which a throw discards. This is the coroutine equivalent of
// yield[ec] rather than a bare yield.
constexpr auto nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

asio::awaitable<void> select_one(ozo::connection_info<> conn_info, asio::io_context& io) {
    using namespace ozo::literals;
    using namespace std::chrono_literals;

    ozo::rows_of<int> result;
    auto [ec, connection] = co_await ozo::request(
        conn_info[io], "SELECT 1"_SQL, 1s, ozo::into(result), nothrow_awaitable);

    if (ec) {
        // The connection survived the failure, so its diagnostics are reachable.
        std::cerr << ec.message() << ": " << ozo::error_message(connection)
                  << " " << ozo::get_error_context(connection) << std::endl;
        co_return;
    }

    for (auto value : result) {
        std::cout << std::get<0>(value) << std::endl;
    }
}
```

The connection yielded by `co_await` is itself a `ConnectionProvider`, so it can
be passed straight to the next operation. Requests, transactions, the connection
pool and time constraints all work this way; see
[examples/coroutine.cpp](examples/coroutine.cpp) and
[the coroutine tests](tests/integration/coroutine_integration.cpp).

## Provenance

OZO was originally developed at Yandex and released under the PostgreSQL License
in 2018. Upstream development stopped in 2021. This is an independent fork,
maintained separately, with no affiliation to or involvement from any Yandex
entity. The original copyright notice is retained as the license requires.

## Compatibilities

**Boost 1.88 or newer is required.** OZO uses Boost.Asio's default executor model;
the `BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT` workaround required by earlier versions is
**no longer needed and must not be defined** — the legacy TS executor was removed from
Boost. The library carries no Boost version-conditional code at all.

Strictly the code needs 1.86, where `boost::uuids::uuid` gained its current
representation. The minimum is 1.88 because that is the oldest Boost packaged by a
currently supported distribution, so every supported configuration can be built and
tested against distribution packages rather than a hand-built Boost.

`apt install libboost-all-dev` is therefore enough on Ubuntu 26.04 LTS (Boost 1.90)
and Ubuntu 25.10 (Boost 1.88). On older releases — notably Ubuntu 24.04 LTS, which
ships 1.83 — Boost has to come from upstream, Homebrew, Conan or vcpkg.

Verified against Boost 1.90, libpq 18, PostgreSQL 18, CMake 4.4 and C++17 on
Apple Clang (arm64). CI covers GCC and Clang on Ubuntu 26.04 and macOS, builds the
1.88 minimum in an Ubuntu 25.10 container, and runs the integration suite against
PostgreSQL 14 and 17; see [the workflow](.github/workflows/ci.yml) for the exact set.

## Dependencies

These things are needed:

* **CMake** is used as build system
* **GCC** or **Clang** C++ compiler with C++17 support (tested with GCC 7.0, Clang 5.0 and Apple LLVM version 9.0.0)
* **Boost** >= 1.86 with `BOOST_HANA_CONFIG_ENABLE_STRING_UDL` defined.
* **libpq** >= 9.3
* [resource_pool](https://github.com/elsid/resource_pool) is vendored in `contrib`, so there is
  nothing to install and no submodule to initialise. It is MIT licensed, copyright Roman Siromakha;
  see `contrib/resource_pool/LICENSE`, which is installed and packaged alongside OZO's own licence.

If you want to run integration tests and/or build inside Docker container:
* **Docker** >= 1.13.0
* **Docker Compose** >= 1.10.0

## Build

The library is header-only, but if you want to build and run unit-tests you can do it as listed below.

### Build and run tests on custom environment

First of all you need to satsfy requirements listed above. You can run tests using these commands.

```bash
mkdir -p build
cd build
cmake .. -DOZO_BUILD_TESTS=ON
make -j$(nproc)
ctest -V
```

Or use [build.sh](scripts/build.sh) which accepts folowing commands:

```bash
scripts/build.sh help
```

prints help.

```bash
scripts/build.sh <compiler> <target>
```

build and run tests with specified **compiler** and **target**, the **compiler** parameter can be:

* **gcc** - for build with gcc,
* **clang** - for build with clang.

The **target** parameter depends on **compiler**.
For **gcc**:

* **debug** - for debug build and tests
* **release** - for release build and tests
* **coverage** - for code coverage calculation

For **clang**:

* **debug** - for debug build and tests
* **release** - for release build and tests
* **asan** - for [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html) launch
* **ubsan** - for [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html) launch
* **tsan** - for [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html) launch

```bash
scripts/build.sh all
```

build all possible configuration.

```bash
scripts/build.sh docs
```

generates documentation.

### Build and run tests on MacOS 10.X

For MacOS the best way to satisfy minimum requirements is [brew](https://brew.sh/)

```bash
brew install cmake boost libpq postresql
```

### Build and run tests within Docker

To build code and run tests inside docker container:

```bash
scripts/build.sh docker <compiler> <target>
```

To generate documentation using docker container:

```bash
scripts/build.sh docker docs
```

### Test against a local postgres

You can use `scripts/build.sh` but add `pg` first:

```bash
scripts/build.sh pg <compiler> <target>
```

or if you want build code in docker:

```bash
scripts/build.sh pg docker <compiler> <target>
```

This will attempt to launch postgres:alpine from your Docker registry.
Or you can point ozo tests to a postgres of your choosing by setting these environment variables prior to building:

```bash
export OZO_BUILD_PG_TESTS=ON
export OZO_PG_TEST_CONNINFO='your conninfo (connection string)'

scripts/build.sh gcc debug
```
