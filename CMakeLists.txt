cmake_minimum_required(VERSION 3.25)
project(tape_scanner LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

# BUILD_CORE — ядро и обязательный ctest (test_scan_geometry), без wxWidgets и SDK камеры.
# BUILD_APP — окно. Если включено, отсутствие зависимости прерывает конфигурацию.
# BUILD_HARDWARE_TESTS — сборка аппаратных и интерактивных программ. В обычный ctest они не входят.
# SCANNER_RUN_HARDWARE и непустой SCANNER_STAND_ID регистрируют только аппаратные тесты
# (метка hardware). Обычный ctest их не запускает.
if(WIN32)
    set(TAPE_SCANNER_DEFAULT_BUILD_APP ON)
else()
    set(TAPE_SCANNER_DEFAULT_BUILD_APP OFF)
endif()

option(BUILD_CORE "Build the scan core and the mandatory test_scan_geometry ctest" ON)
option(BUILD_APP "Build the desktop application. Missing dependencies are a configure error." ${TAPE_SCANNER_DEFAULT_BUILD_APP})
option(BUILD_HARDWARE_TESTS "Build hardware and interactive programs. They are not part of ordinary ctest." OFF)
option(SCANNER_RUN_HARDWARE "Register hardware tests with CTest. Requires SCANNER_STAND_ID. Ordinary ctest does not use this." OFF)
set(SCANNER_STAND_ID "" CACHE STRING "Stand identifier. Required when SCANNER_RUN_HARDWARE is ON.")

set(CAMERA_SDK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/MVCAMSDK_X64.lib" CACHE FILEPATH
    "MindVision/HuaTeng MVCAMSDK x64 import library shipped in the repository.")
set(CAMERA_SDK_DLL "${CMAKE_CURRENT_SOURCE_DIR}/lib/MVCAMSDK_X64.dll" CACHE FILEPATH
    "MindVision/HuaTeng MVCAMSDK x64 runtime shipped in the repository.")
# SHA-256 of the libraries committed for the MSVC x64 profile. vcpkg does not provide this SDK.
set(CAMERA_SDK_LIB_SHA256 "05c10dce3cab7737a7a288f842c50f5a7f6eabd2dcdc2963f5a85fcdb895cabd")
set(CAMERA_SDK_DLL_SHA256 "f6cf90c22b59e36403576a9914757713cb5851a5bda35c48542428a4dc51ba2e")

if(NOT BUILD_CORE AND NOT BUILD_APP AND NOT BUILD_HARDWARE_TESTS)
    message(FATAL_ERROR
        "Nothing to build. Enable at least one of BUILD_CORE, BUILD_APP, BUILD_HARDWARE_TESTS.")
endif()

if(SCANNER_RUN_HARDWARE AND NOT BUILD_HARDWARE_TESTS)
    message(FATAL_ERROR
        "SCANNER_RUN_HARDWARE=ON requires BUILD_HARDWARE_TESTS=ON. "
        "Ordinary ctest does not run hardware tests; leave SCANNER_RUN_HARDWARE=OFF for the mandatory suite.")
endif()

if(SCANNER_RUN_HARDWARE AND SCANNER_STAND_ID STREQUAL "")
    message(FATAL_ERROR
        "Hardware CTest registration requires a non-empty SCANNER_STAND_ID "
        "(for example -DSCANNER_STAND_ID=bench-01).")
endif()

find_package(OpenCV REQUIRED)
find_package(DCMTK CONFIG QUIET)
if(NOT DCMTK_FOUND)
    find_package(DCMTK REQUIRED)
endif()

if(BUILD_CORE)
    add_library(tape_scanner_scan STATIC
        src/scanner/Utils.cpp
        src/scanner/Film.cpp
        src/scanner/Region.cpp
        src/scanner/ScanSession.cpp
        src/scanner/DicomSeries.cpp
        src/scanner/WindowLevel.cpp
    )
    target_include_directories(tape_scanner_scan PUBLIC "include/")
    target_link_libraries(tape_scanner_scan PUBLIC ${OpenCV_LIBS})
    if(OpenCV_INCLUDE_DIRS)
        target_include_directories(tape_scanner_scan PUBLIC ${OpenCV_INCLUDE_DIRS})
    endif()
    if(TARGET DCMTK::dcmdata)
        target_link_libraries(tape_scanner_scan PUBLIC DCMTK::dcmdata)
    else()
        target_include_directories(tape_scanner_scan PUBLIC ${DCMTK_INCLUDE_DIRS})
        target_link_libraries(tape_scanner_scan PUBLIC ${DCMTK_LIBRARIES})
    endif()
    if(MSVC)
        target_compile_options(tape_scanner_scan PRIVATE /utf-8)
    endif()

    if(BUILD_TESTING)
        add_executable(test_scan_geometry tests/test_scan_geometry.cpp)
        target_link_libraries(test_scan_geometry PRIVATE tape_scanner_scan)
        if(MSVC)
            target_compile_options(test_scan_geometry PRIVATE /utf-8)
        endif()
        add_test(NAME test_scan_geometry COMMAND test_scan_geometry)
        set_tests_properties(test_scan_geometry PROPERTIES
            LABELS "mandatory"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()
endif()

if(BUILD_APP OR BUILD_HARDWARE_TESTS)
    find_package(wxWidgets QUIET COMPONENTS net core base gl)
    find_package(OpenGL QUIET)
    find_package(glm CONFIG QUIET)

    set(tape_scanner_missing "")
    if(NOT WIN32)
        list(APPEND tape_scanner_missing "Windows MSVC x64 for MVCAMSDK_X64.lib")
    endif()
    if(NOT wxWidgets_FOUND)
        list(APPEND tape_scanner_missing "wxWidgets (components net, core, base, gl)")
    endif()
    if(NOT OpenGL_FOUND)
        list(APPEND tape_scanner_missing "OpenGL")
    endif()
    if(NOT glm_FOUND)
        list(APPEND tape_scanner_missing "glm")
    endif()
    if(NOT EXISTS "${CAMERA_SDK_LIB}")
        list(APPEND tape_scanner_missing "camera SDK import library (${CAMERA_SDK_LIB})")
    endif()
    if(NOT EXISTS "${CAMERA_SDK_DLL}")
        list(APPEND tape_scanner_missing "camera SDK runtime (${CAMERA_SDK_DLL})")
    endif()
    if(tape_scanner_missing)
        list(JOIN tape_scanner_missing "\n  - " tape_scanner_missing_text)
        message(FATAL_ERROR
            "BUILD_APP or BUILD_HARDWARE_TESTS is ON, but required dependencies are missing.\n"
            "Configuration stops instead of skipping the target.\n"
            "  - ${tape_scanner_missing_text}\n"
            "Mandatory tests do not need these dependencies:\n"
            "  cmake -DBUILD_APP=OFF -DBUILD_HARDWARE_TESTS=OFF -S . -B build\n"
            "  cmake --build build && ctest --test-dir build --output-on-failure")
    endif()

    function(tape_scanner_check_sdk_hash path expected label)
        file(SHA256 "${path}" actual)
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR
                "${label} checksum mismatch for ${path}.\n"
                "  expected ${expected}\n"
                "  actual   ${actual}\n"
                "Use the MVCAMSDK_X64 files shipped in lib/. vcpkg does not install this SDK.")
        endif()
    endfunction()
    tape_scanner_check_sdk_hash("${CAMERA_SDK_LIB}" "${CAMERA_SDK_LIB_SHA256}" "MVCAMSDK_X64.lib")
    tape_scanner_check_sdk_hash("${CAMERA_SDK_DLL}" "${CAMERA_SDK_DLL_SHA256}" "MVCAMSDK_X64.dll")

    include(${wxWidgets_USE_FILE})

    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "src/*.c*")
    list(FILTER sources EXCLUDE REGEX ".*main\\.cpp$")
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS "include/*.h*")

    file(GLOB files_to_copy
        "src/dicom_viewer/shaders/*"
        "resources/*"
    )

    set(copied_files "")
    foreach(copying_file ${files_to_copy})
        get_filename_component(file_name ${copying_file} NAME)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file_name}
            COMMAND ${CMAKE_COMMAND}
                    -E copy_if_different
                    ${copying_file}
                    ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS ${copying_file}
            COMMENT "Copying file ${copying_file}"
            VERBATIM
        )
        list(APPEND copied_files ${CMAKE_CURRENT_BINARY_DIR}/${file_name})
    endforeach()

    add_library(tape_scanner_lib STATIC ${sources} ${headers})
    target_include_directories(tape_scanner_lib PRIVATE "include/")
    target_compile_definitions(tape_scanner_lib PRIVATE TAPE_SCANNER_USE_WX)
    target_link_libraries(tape_scanner_lib
        ${wxWidgets_LIBRARIES}
        ${OpenCV_LIBS}
        OpenGL::GL
        glm::glm
        ${CAMERA_SDK_LIB}
    )
    if(WIN32)
        target_link_libraries(tape_scanner_lib ws2_32)
    endif()
    if(TARGET DCMTK::dcmdata)
        target_link_libraries(tape_scanner_lib DCMTK::dcmdata)
        if(TARGET DCMTK::dcmimage)
            target_link_libraries(tape_scanner_lib DCMTK::dcmimage)
        endif()
        if(TARGET DCMTK::dcmimgle)
            target_link_libraries(tape_scanner_lib DCMTK::dcmimgle)
        endif()
        if(TARGET DCMTK::dcmjpeg)
            target_link_libraries(tape_scanner_lib DCMTK::dcmjpeg)
        endif()
        if(TARGET DCMTK::i2d)
            target_link_libraries(tape_scanner_lib DCMTK::i2d)
        endif()
    else()
        target_include_directories(tape_scanner_lib PRIVATE ${DCMTK_INCLUDE_DIRS})
        target_link_libraries(tape_scanner_lib ${DCMTK_LIBRARIES})
    endif()
    if(MSVC)
        target_compile_options(tape_scanner_lib PRIVATE /utf-8)
    endif()

    if(BUILD_APP)
        add_executable(tape_scanner WIN32
            src/main.cpp
            ${copied_files}
        )
        target_include_directories(tape_scanner PRIVATE "include/")
        target_link_libraries(tape_scanner PRIVATE tape_scanner_lib)
        add_custom_command(TARGET tape_scanner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CAMERA_SDK_DLL}"
                "$<TARGET_FILE_DIR:tape_scanner>/MVCAMSDK_X64.dll"
            COMMENT "Copying MVCAMSDK_X64.dll next to tape_scanner.exe")
        foreach(runtime_file ${files_to_copy})
            add_custom_command(TARGET tape_scanner POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${runtime_file}"
                    "$<TARGET_FILE_DIR:tape_scanner>"
                COMMENT "Copying ${runtime_file} next to tape_scanner.exe")
        endforeach()
    endif()

    if(BUILD_HARDWARE_TESTS)
        function(tape_scanner_add_manual_program program_name)
            add_executable(${program_name} tests/${program_name}.cpp)
            target_include_directories(${program_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
            target_link_libraries(${program_name} PRIVATE tape_scanner_lib)
            if(MSVC)
                target_compile_options(${program_name} PRIVATE /utf-8)
            endif()
        endfunction()

        # Интерактивные программы (ожидание клавиши, просмотр). В ctest не регистрируются.
        foreach(manual_program IN ITEMS
                test_film test_region test_region_reverse test_analyze_cadr test_save_dcm)
            tape_scanner_add_manual_program(${manual_program})
        endforeach()

        # Аппаратные программы: камера и движение. В обычный ctest не входят.
        foreach(hardware_program IN ITEMS test_hdr test_exposes test_scale)
            tape_scanner_add_manual_program(${hardware_program})
            if(SCANNER_RUN_HARDWARE)
                add_test(NAME ${hardware_program} COMMAND ${hardware_program})
                set_tests_properties(${hardware_program} PROPERTIES
                    LABELS "hardware"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                    ENVIRONMENT "SCANNER_STAND_ID=${SCANNER_STAND_ID}"
                )
            endif()
        endforeach()
    endif()
endif()

message(STATUS "tape_scanner BUILD_CORE=${BUILD_CORE} BUILD_APP=${BUILD_APP} BUILD_HARDWARE_TESTS=${BUILD_HARDWARE_TESTS} SCANNER_RUN_HARDWARE=${SCANNER_RUN_HARDWARE}")
if(BUILD_TESTING AND BUILD_CORE)
    message(STATUS "Mandatory ctest: test_scan_geometry (label mandatory). Hardware tests are excluded unless SCANNER_RUN_HARDWARE=ON and SCANNER_STAND_ID is set; then select them with ctest -L hardware.")
endif()
