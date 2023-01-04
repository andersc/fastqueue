cmake_minimum_required(VERSION 3.10)
project(fast_queue)
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_OSX_ARCHITECTURES "x86_64") #for testing x86_64 on my mac
message(STATUS "Building for architecture: ${CMAKE_SYSTEM_PROCESSOR}")

find_package (Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deaod_spsc/)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        # Visual Studio specific
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    else()
        # Other compilers
        #SET(CMAKE_CXX_FLAGS "-mavx2")
    endif()
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64)|(arm64)")
else ()
    message( FATAL_ERROR "Not supported architecture, X86_64 and arm64 is." )
endif ()

add_executable(fast_queue_integrity_test FastQueueIntegrityTest.cpp)
target_link_libraries(fast_queue_integrity_test Threads::Threads)

#cmake -DUSE_BOOST=ON ..
#to compile the code comparing against boost::lockfree::spsc_queue and rigtorp
if(USE_BOOST)
    find_package (Boost REQUIRED)
    include_directories(${Boost_INCLUDE_DIR})

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)" OR CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")

        message("Building X86_64")
        enable_language(ASM_NASM)

        if(APPLE)
            string(APPEND CMAKE_ASM_NASM_FLAGS "--prefix _")
        endif(APPLE)

        if(WIN32)
            string(APPEND CMAKE_ASM_NASM_FLAGS "-dWIN32=1")
        endif(WIN32)

        add_executable(fast_queue_compare FastQueueCompare.cpp fastqueue_x86_64.asm)

    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64)|(arm64)")

        message("Building ARM64")
        enable_language(ASM)

        add_executable(fast_queue_compare FastQueueCompare.cpp fastqueue_arm64.asm)

    else ()
        message( FATAL_ERROR "Not supported architecture, X86_64 and arm64 is." )
    endif ()


    if (Boost_LIBRARIES STREQUAL "")
        message(STATUS "Boost_LIBRARIES string is empty. Will try to compile anyway.")
        #comment contains an alternative method if BOOST not found by CMake
        #get_filename_component(FQBoostRoot "${Boost_INCLUDE_DIR}" DIRECTORY)
        #message("${FQBoostRoot}/lib")
        #link_directories(${FQBoostRoot}/lib)
        #target_link_libraries(fast_queue_compare boost_system Threads::Threads)
        target_link_libraries(fast_queue_compare Threads::Threads)
    else()
        target_link_libraries(fast_queue_compare Threads::Threads ${Boost_LIBRARIES})
    endif()
endif(USE_BOOST)
