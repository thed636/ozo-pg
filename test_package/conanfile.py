import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class OzoTestConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        # The example needs a live database to do anything, so running it is not
        # part of the test. Building and linking it is what matters here: it
        # proves find_package(ozo) resolves, that ozo::ozo is usable as a target
        # and that the packaged headers are complete.
        if can_run(self):
            self.run(os.path.join(self.cpp.build.bindir, "example"), env="conanrun")
