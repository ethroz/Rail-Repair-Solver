from conan import ConanFile
from conan.errors import ConanException
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class rail_repair_solverRecipe(ConanFile):
    name = "rail_repair_solver"
    version = "0.1"
    package_type = "application"

    # Optional metadata
    author = "Ethan Rozee ethroz@gmail.com"
    description = "A Solver for the game Rail Repair on Fancade"
    topics = ("solver", "sokoban", "fancade")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    
    requires = [
        "abseil/20260107.1"
    ]
    options = {
        "verbose": [True, False]
    }
    default_options = {
        "verbose": False,
    }

    def export(self):
        raise ConanException("This recipe cannot be exported")

    def layout(self):
        cmake_layout(self)
        self.cpp.source.includedirs = ["src", "thirdparty"]

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["VERBOSE_LOGS"] = self.options.verbose
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
