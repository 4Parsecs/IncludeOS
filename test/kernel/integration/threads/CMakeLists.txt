cmake_minimum_required(VERSION 3.0)

# Service
project (smp_test)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake OPTIONAL RESULT_VARIABLE HAS_CONAN)
if (NOT HAS_CONAN)
  message(FATAL_ERROR "missing conanbuildinfo.cmake did you forget to run conan install ?")
endif()
conan_basic_setup()

include(os)

set(SOURCES
    service.cpp # ...add more here
)

os_add_executable(kernel_threads "Threads test" ${SOURCES})
os_add_stdout(kernel_threads default_stdout)
os_add_drivers(kernel_threads boot_logger)
#os_add_plugins(service vfs)

configure_file(test.py ${CMAKE_CURRENT_BINARY_DIR})
