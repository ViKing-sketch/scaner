cmake_minimum_required(VERSION 3.16)
project(tape_scanner)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Геометрия скана и DICOM-сегменты собираются отдельно от окна, OpenGL и SDK камеры.
# Юнит-тест test_scan_geometry камеру и включённый сканер не использует.
find_package(OpenCV REQUIRED)
find_package(DCMTK CONFIG QUIET)
if(NOT DCMTK_FOUND)
    find_package(DCMTK REQUIRED)
endif()

add_library(tape_scanner_scan STATIC
    src/scanner/Utils.cpp
    src/scanner/Film.cpp
    src/scanner/Region.cpp
    src/scanner/ScanSession.cpp
    src/scanner/DicomSeries.cpp
    src/scanner/DicomScan.cpp
    src/scanner/WindowLevel.cpp
)
target_include_directories(tape_scanner_scan PUBLIC "include/")
target_link_libraries(tape_scanner_scan PUBLIC ${OpenCV_LIBS})
if(OpenCV_INCLUDE_DIRS)
    target_include_directories(tape_scanner_scan PUBLIC ${OpenCV_INCLUDE_DIRS})
endif()
if(TARGET DCMTK::dcmdata)
    target_link_libraries(tape_scanner_scan PUBLIC DCMTK::dcmdata)
    if(TARGET DCMTK::dcmimgle)
        target_link_libraries(tape_scanner_scan PUBLIC DCMTK::dcmimgle)
    endif()
    if(TARGET DCMTK::dcmimage)
        target_link_libraries(tape_scanner_scan PUBLIC DCMTK::dcmimage)
    endif()
    if(TARGET DCMTK::dcmjpeg)
        target_link_libraries(tape_scanner_scan PUBLIC DCMTK::dcmjpeg)
    endif()
else()
    target_include_directories(tape_scanner_scan PUBLIC ${DCMTK_INCLUDE_DIRS})
    target_link_libraries(tape_scanner_scan PUBLIC ${DCMTK_LIBRARIES})
endif()
if(MSVC)
    target_compile_options(tape_scanner_scan PRIVATE /utf-8)
endif()

enable_testing()

add_executable(test_scan_geometry tests/test_scan_geometry.cpp)
target_link_libraries(test_scan_geometry PRIVATE tape_scanner_scan)
if(MSVC)
    target_compile_options(test_scan_geometry PRIVATE /utf-8)
endif()
add_test(NAME test_scan_geometry COMMAND test_scan_geometry)

add_executable(test_open_scan tests/test_open_scan.cpp src/dicom_generator/BinToDcmConverter.cpp)
target_link_libraries(test_open_scan PRIVATE tape_scanner_scan)
target_compile_definitions(test_open_scan PRIVATE TAPE_SCANNER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(MSVC)
    target_compile_options(test_open_scan PRIVATE /utf-8)
endif()
add_test(NAME test_open_scan COMMAND test_open_scan)

find_package(wxWidgets QUIET COMPONENTS net core base gl)
find_package(OpenGL QUIET)
find_package(glm CONFIG QUIET)
set(CAMERA_SDK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/MVCAMSDK_X64.lib")

if(wxWidgets_FOUND AND OpenGL_FOUND AND glm_FOUND AND EXISTS "${CAMERA_SDK_LIB}")
    include(${wxWidgets_USE_FILE})

    file(GLOB_RECURSE sources
        "src/*.c*"
    )
    list(FILTER sources EXCLUDE REGEX ".*main\\.cpp$")

    file(GLOB_RECURSE headers
        "include/*.h*"
    )

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
        ws2_32
        ${CAMERA_SDK_LIB}
    )
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

    add_executable(tape_scanner WIN32
        src/main.cpp
        ${copied_files}
    )
    target_include_directories(tape_scanner PRIVATE "include/")
    target_link_libraries(tape_scanner PRIVATE tape_scanner_lib)
    add_custom_command(TARGET tape_scanner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/MVCAMSDK_X64.dll"
            "$<TARGET_FILE_DIR:tape_scanner>/MVCAMSDK_X64.dll"
        COMMENT "Copying MVCAMSDK_X64.dll next to tape_scanner.exe")
    foreach(runtime_file ${files_to_copy})
        add_custom_command(TARGET tape_scanner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${runtime_file}"
                "$<TARGET_FILE_DIR:tape_scanner>"
            COMMENT "Copying ${runtime_file} next to tape_scanner.exe")
    endforeach()
    foreach(runtime_file ${files_to_copy})
        add_custom_command(TARGET tape_scanner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${runtime_file}"
                "$<TARGET_FILE_DIR:tape_scanner>"
            COMMENT "Copying ${runtime_file} next to tape_scanner.exe")
    endforeach()

    function(add_simple_test test_name)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_include_directories(${test_name} PRIVATE "include/")
        target_link_libraries(${test_name} PRIVATE tape_scanner_lib)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endfunction()

    add_simple_test(test_film)
    add_simple_test(test_region)
    add_simple_test(test_region_reverse)
    add_simple_test(test_analyze_cadr)
    add_simple_test(test_save_dcm)
    add_simple_test(test_hdr)
    add_simple_test(test_exposes)
    add_simple_test(test_scale)
else()
    message(STATUS
        "UI scanner target is skipped: wxWidgets, OpenGL, glm or the camera SDK is missing. "
        "test_scan_geometry still builds and does not need the scanner to be connected.")
endif()
