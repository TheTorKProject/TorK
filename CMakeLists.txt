#------------------------------------------------------------------------------
# General Settings
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)
project(tork)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_BUILD_FILES_DIRECTORY ${PROJECT_SOURCE_DIR}/build/)
set(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/build/)
#add_compile_options(-Wall)

#------------------------------------------------------------------------------
# Check Dependencies
#------------------------------------------------------------------------------

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem unit_test_framework)
find_package(OpenSSL REQUIRED)

#------------------------------------------------------------------------------
# Includes
#------------------------------------------------------------------------------

include_directories("${PROJECT_SOURCE_DIR}/src")

#------------------------------------------------------------------------------
# Executables and Libraries
#------------------------------------------------------------------------------

add_executable(${PROJECT_NAME}
        src/tork.cc
)

add_executable(cli
        src/cli.cc
)

add_executable(testk
        src/test.cc
)

add_library(torkl
        src/cli/CliUnixClient.hh
        src/cli/CliUnixClient.cc
        src/cli/CliUnixServer.hh
        src/cli/CliUnixServer.cc
        src/tordriver/TorPT.hh
        src/tordriver/TorPT.cc
        src/tordriver/TorPTClient.hh
        src/tordriver/TorPTClient.cc
        src/tordriver/TorPTServer.hh
        src/tordriver/TorPTServer.cc
        src/tordriver/TorController.hh
        src/tordriver/TorController.cc
        src/tordriver/SocksProxyClient.hh
        src/tordriver/SocksProxyClient.cc
        src/tordriver/SocksProxyServer.hh
        src/tordriver/SocksProxyServer.cc
        src/controller/Controller.hh
        src/controller/ControllerClient.hh
        src/controller/ControllerClient.cc
        src/controller/ControllerServer.hh
        src/controller/ControllerServer.cc
        src/controller/FdPair.hh
        src/controller/Frame.hh
        src/controller/Frame.cc
        src/controller/FramePool.hh
        src/controller/FramePool.cc
        src/controller/FrameQueue.hh
        src/controller/FrameQueue.cc
        src/controller/TrafficShaper.hh
        src/controller/TrafficShaper.cc
        src/controller/ClientManager.hh
        src/controller/ClientManager.cc
        src/controller/Client.hh
        src/common/Common.hh
        src/common/Common.cc
        src/common/cmdline.h
        src/common/RingBuffer.hh
        src/common/RingBuffer.cc
        src/common/SSL.hh
)

#------------------------------------------------------------------------------
# Dependencies
#------------------------------------------------------------------------------

target_link_libraries(${PROJECT_NAME} torkl)
target_link_libraries(${PROJECT_NAME} Boost::system Boost::filesystem)
target_link_libraries(${PROJECT_NAME} Threads::Threads)
target_link_libraries(${PROJECT_NAME} ssl)
target_link_libraries(${PROJECT_NAME} crypto)

target_link_libraries(cli torkl)

target_link_libraries(testk torkl)
target_link_libraries(testk Threads::Threads)
