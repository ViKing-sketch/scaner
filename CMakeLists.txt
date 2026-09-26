cmake_minimum_required(VERSION 3.10)
project(tape_scanner)

set(CMAKE_CXX_STANDARD 20)

find_package(wxWidgets REQUIRED COMPONENTS net core base gl)
    include(${wxWidgets_USE_FILE})
find_package(OpenCV REQUIRED)
find_package(DCMTK REQUIRED)
find_package(OpenGL REQUIRED)

# Рекурсивно находим все исходники
file(GLOB_RECURSE sources
    "src/*.c*"
)
list(FILTER sources EXCLUDE REGEX ".*main\\.cpp$")  # исключаем main.cpp

# Заголовочные файлы исключительно для отображения их в IDE
file(GLOB_RECURSE headers
    "include/*.h*"
)

# Копируем шейдеры в сборочный каталог
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

add_library(tape_scanner_lib STATIC ${sources})
target_include_directories(tape_scanner_lib PRIVATE "include/")
target_link_libraries(tape_scanner_lib
    ${wxWidgets_LIBRARIES}
    ${OpenCV_LIBS}
    ${DCMTK_LIBRARIES}
    OpenGL::GL
    ws2_32
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/MVCAMSDK_X64.lib
)

# Собираем исполняемый файл
add_executable(tape_scanner
    src/main.cpp
    ${copied_files}
)
target_include_directories(tape_scanner PRIVATE "include/")
target_link_libraries(tape_scanner PRIVATE tape_scanner_lib)

enable_testing()

# Функция для добавления одного теста
function(add_simple_test test_name)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_include_directories(${test_name} PRIVATE "include/")
    target_link_libraries(${test_name} PRIVATE
        tape_scanner_lib
    )
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
