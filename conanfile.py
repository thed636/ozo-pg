import os
import re

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy, load


class OzoConan(ConanFile):
    name = "ozo"
    license = "PostgreSQL"
    url = "https://github.com/thed636/ozo-pg"
    homepage = "https://github.com/thed636/ozo-pg"
    description = "Header-only C++17 async PostgreSQL client built on Boost.Asio"
    topics = ("ozo", "postgres", "postgresql", "cpp17", "database", "db", "asio",
              "async", "header-only")

    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    exports_sources = "include/*", "contrib/resource_pool/include/*", \
                      "contrib/resource_pool/LICENSE", "LICENSE", "AUTHORS"

    def set_version(self):
        # Keep CMakeLists.txt the single source of truth for the version rather
        # than repeating it here where the two would drift apart.
        content = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
        match = re.search(r"^\s*project\(ozo\s+VERSION\s+([^\s)]+)", content, re.M)
        if match is None:
            raise ConanInvalidConfiguration("cannot determine the version from CMakeLists.txt")
        self.version = match.group(1).strip()

    def requirements(self):
        # Version ranges rather than exact pins: OZO's contract is Boost 1.88 or
        # newer, and pinning one patch release is how the previous recipe ended
        # up stranded on Boost 1.74.
        #
        # transitive_headers is required because these appear in OZO's own
        # public headers, so consumers must see them too.
        #
        # resource_pool is deliberately absent: it is vendored in contrib and
        # packaged with OZO. Conan Center only carries a 2021 snapshot of it,
        # pinned to Boost 1.79 and predating the port to asio::any_io_executor,
        # which would both conflict with the requirement below and fail to
        # compile. See contrib/CMakeLists.txt.
        self.requires("boost/[>=1.88 <2]", transitive_headers=True)
        self.requires("libpq/[>=14 <18]", transitive_headers=True)

    def package_id(self):
        # Header-only: the resulting package is identical for every
        # configuration, so it must not depend on settings or on the dependency
        # binaries.
        self.info.clear()

    def validate(self):
        if self.settings.os == "Windows":
            raise ConanInvalidConfiguration("OZO is not compatible with Windows")
        check_min_cppstd(self, 17)

    def package(self):
        copy(self, "LICENSE", self.source_folder,
             os.path.join(self.package_folder, "licenses"))
        copy(self, "AUTHORS", self.source_folder,
             os.path.join(self.package_folder, "licenses"))
        # The vendored resource_pool is MIT licensed and separately copyrighted,
        # so its notice ships alongside OZO's own rather than being folded into it.
        copy(self, "LICENSE", os.path.join(self.source_folder, "contrib", "resource_pool"),
             os.path.join(self.package_folder, "licenses"),
             rename="LICENSE.resource_pool")

        copy(self, "*", os.path.join(self.source_folder, "include"),
             os.path.join(self.package_folder, "include"))
        copy(self, "*", os.path.join(self.source_folder, "contrib", "resource_pool", "include"),
             os.path.join(self.package_folder, "include"))

    def package_info(self):
        # Nothing is built, so there is neither a library nor a binary directory.
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # find_package(ozo) providing the ozo::ozo target, matching what the
        # project's own CMake exports. The package deliberately does not ship
        # OZO's ozo-config.cmake: under Conan the consumer uses the config that
        # CMakeDeps generates, and shipping a second one invites the two to be
        # found in an unpredictable order.
        self.cpp_info.set_property("cmake_file_name", "ozo")
        self.cpp_info.set_property("cmake_target_name", "ozo::ozo")

        self.cpp_info.defines = [
            "BOOST_COROUTINES_NO_DEPRECATION_WARNING",
            "BOOST_HANA_CONFIG_ENABLE_STRING_UDL",
        ]

        self.cpp_info.requires = [
            "boost::headers",
            "boost::coroutine",
            "boost::context",
            "boost::thread",
            "boost::atomic",
            "libpq::pq",
        ]

        if self.settings.compiler in ("clang", "apple-clang"):
            # OZO_PG_DEFINE_CUSTOM_TYPE expands to a variadic macro invoked with
            # no variadic arguments, which is a GNU extension.
            self.cpp_info.cxxflags = ["-Wno-gnu-zero-variadic-macro-arguments"]
