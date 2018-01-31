from conans import ConanFile, CMake, tools


class BredisConan(ConanFile):
    name = "bredis"
    version = "0.05"
    license = "<Put the package license here>"
    url = "https://github.com/basiliscos/cpp-bredis"
    description = "MIT"
    settings = "os", "compiler", "build_type", "arch"

    requires = "boost/1.66.0@conan/stable"
    default_options = (
        "boost:shared=True",
        "boost:header_only=False",
        "boost:without_math=True",
        "boost:without_wave=True",
        "boost:without_container=True",
        "boost:without_exception=True",
        "boost:without_graph=True",
        "boost:without_iostreams=True",
        "boost:without_locale=True",
        "boost:without_log=True",
        "boost:without_program_options=True",
        "boost:without_random=True",
        "boost:without_regex=True",
        "boost:without_mpi=True",
        "boost:without_serialization=True",
        "boost:without_signals=True",
        "boost:without_coroutine=False",
        "boost:without_fiber=True",
        "boost:without_context=False",
        "boost:without_timer=True",
        "boost:without_thread=False",
        "boost:without_chrono=True",
        "boost:without_date_time=True",
        "boost:without_atomic=True",
        "boost:without_filesystem=True",
        "boost:without_system=True",
        "boost:without_graph_parallel=True",
        "boost:without_python=True",
        "boost:without_stacktrace=True",
        "boost:without_test=True",
        "boost:without_type_erasure=True",
    )

    exports_sources = "include/*", "CMakeLists.txt", "examples/speed_test_async_multi.cpp"
    no_copy_source = True

    generators = "cmake"
    exports = "CMakeLists.txt", "include*", "examples*", "t*"

    # this is not building a library, just tests
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()
    def package(self):
        self.copy("*.hpp", dst="include", src="include")
        self.copy("*.ipp", dst="include", src="include")

    def package_id(self):
        self.info.header_only()

    #def package_info(self):
    #    self.cpp_info.libs = ["cpp-bredis"]
