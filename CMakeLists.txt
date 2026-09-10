cmake_minimum_required(VERSION 3.16)
project(GradeGoal LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(gg_warnings INTERFACE)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gg_warnings INTERFACE
        -std=c17
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
    )
    if(ENABLE_ASAN)
        target_compile_options(gg_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(gg_warnings INTERFACE -fsanitize=address,undefined)
    endif()
elseif(MSVC)
    target_compile_options(gg_warnings INTERFACE
        /std:c17
        /W4
        /WX
        /wd4996
    )
    if(ENABLE_ASAN)
        target_compile_options(gg_warnings INTERFACE /fsanitize=address)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Unity/unity.c")
    add_library(unity STATIC third_party/Unity/unity.c)
    target_include_directories(unity SYSTEM PUBLIC third_party/Unity)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
endif()

add_library(gg_core STATIC
    src/core/course_list.c
    src/core/scale_validator.c
    src/core/cgpa_calculator.c
    src/core/target_solver.c
)
target_include_directories(gg_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core>
)
target_link_libraries(gg_core PRIVATE gg_warnings)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/persistence/sqlite_connection.c")
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        set(GG_SQLITE_TARGET SQLite::SQLite3)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/sqlite3.c")
        add_library(sqlite3 STATIC third_party/sqlite3/sqlite3.c)
        target_include_directories(sqlite3 SYSTEM PUBLIC third_party/sqlite3)
        target_compile_definitions(sqlite3 PRIVATE
            SQLITE_THREADSAFE=1
            SQLITE_ENABLE_RTREE=0
            SQLITE_ENABLE_FTS5=0
            SQLITE_OMIT_LOAD_EXTENSION
            SQLITE_DEFAULT_FOREIGN_KEYS=1
        )
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(sqlite3 PRIVATE -w)
        elseif(MSVC)
            target_compile_options(sqlite3 PRIVATE /W0)
        endif()
        if(UNIX)
            target_link_libraries(sqlite3 PUBLIC m ${CMAKE_DL_LIBS})
        endif()
        set(GG_SQLITE_TARGET sqlite3)
    endif()

    if(TARGET ${GG_SQLITE_TARGET})
        add_library(gg_persistence STATIC
            src/persistence/sqlite_connection.c
            src/persistence/sqlite_error.c
            src/persistence/scale_repository.c
            src/persistence/course_repository.c
        )
        target_include_directories(gg_persistence PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/persistence>
        )
        target_link_libraries(gg_persistence
            PUBLIC gg_core ${GG_SQLITE_TARGET}
            PRIVATE gg_warnings
        )
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libxlsxwriter/src/xlsxwriter.c")
    add_library(xlsxwriter STATIC third_party/libxlsxwriter/src/xlsxwriter.c)
    target_include_directories(xlsxwriter PUBLIC third_party/libxlsxwriter/include)
    set(GG_XLSXWRITER_TARGET xlsxwriter)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/xlsxio/src/xlsxio_read.c")
    add_library(xlsxio_read STATIC third_party/xlsxio/src/xlsxio_read.c)
    target_include_directories(xlsxio_read PUBLIC third_party/xlsxio/include)
    set(GG_XLSXIO_TARGET xlsxio_read)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libharu/src/hpdf.c")
    add_library(hpdf STATIC third_party/libharu/src/hpdf.c)
    target_include_directories(hpdf PUBLIC third_party/libharu/include)
    set(GG_HPDF_TARGET hpdf)
endif()

if(TARGET gg_persistence AND TARGET ${GG_XLSXWRITER_TARGET} AND TARGET ${GG_XLSXIO_TARGET} AND TARGET ${GG_HPDF_TARGET})
    add_library(gg_io STATIC
        src/io/io_types.c
        src/io/backup_manager.c
        src/io/xlsx_exporter.c
        src/io/xlsx_importer.c
        src/io/pdf_exporter.c
    )
    target_include_directories(gg_io PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/io>
    )
    target_link_libraries(gg_io
        PUBLIC gg_core gg_persistence ${GG_XLSXWRITER_TARGET} ${GG_XLSXIO_TARGET} ${GG_HPDF_TARGET}
        PRIVATE gg_warnings
    )
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GTK4 QUIET gtk4)
    pkg_check_modules(FONTCONFIG QUIET fontconfig)
endif()

if(GTK4_FOUND AND TARGET gg_io)
    find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/gradegoal_resources.c
            ${CMAKE_CURRENT_BINARY_DIR}/gradegoal_resources.h
        COMMAND ${GLIB_COMPILE_RESOURCES}
            --target=${CMAKE_CURRENT_BINARY_DIR}/gradegoal_resources.c
            --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/assets
            --generate-source
            --c-name gradegoal
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/gradegoal.gresource.xml
        COMMAND ${GLIB_COMPILE_RESOURCES}
            --target=${CMAKE_CURRENT_BINARY_DIR}/gradegoal_resources.h
            --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/assets
            --generate-header
            --c-name gradegoal
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/gradegoal.gresource.xml
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/gradegoal.gresource.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/theme/gradegoal-light.css
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/theme/gradegoal-dark.css
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/PlusJakartaSans/PlusJakartaSans-Regular.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/PlusJakartaSans/PlusJakartaSans-Medium.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/PlusJakartaSans/PlusJakartaSans-SemiBold.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/PlusJakartaSans/PlusJakartaSans-Bold.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/SourceSerif4/SourceSerif4-SemiBold.ttf
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/hicolor/16x16/apps/gradegoal.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/hicolor/32x32/apps/gradegoal.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/hicolor/48x48/apps/gradegoal.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/hicolor/64x64/apps/gradegoal.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/hicolor/128x128/apps/gradegoal.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/hicolor/256x256/apps/gradegoal.png
        VERBATIM
    )

    if(MSVC)
        set_source_files_properties(
            "${CMAKE_CURRENT_BINARY_DIR}/gradegoal_resources.c"
            PROPERTIES
            COMPILE_OPTIONS "/W0"
        )
    else()
        set_source_files_properties(
            "${CMAKE_CURRENT_BINARY_DIR}/gradegoal_resources.c"
            PROPERTIES
            COMPILE_OPTIONS "-w"
        )
    endif()

    add_library(gg_gui STATIC
        src/gui/gg_theme.c
        "${CMAKE_CURRENT_BINARY_DIR}/gradegoal_resources.c"
        src/gui/gg_confirmation_dialog.c
        src/gui/gg_grade_row.c
        src/gui/gg_course_entry_row.c
        src/gui/gg_semester_list_item.c
        src/gui/first_run_wizard.c
        src/gui/dashboard_widget.c
        src/gui/history_widget.c
        src/gui/target_calculator_widget.c
        src/gui/settings_widget.c
        src/gui/main_window.c
        src/gui/app.c
    )
    target_include_directories(gg_gui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/gui>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        ${GTK4_INCLUDE_DIRS}
    )
    target_link_libraries(gg_gui
        PUBLIC gg_core gg_persistence gg_io ${GTK4_LIBRARIES}
        PRIVATE gg_warnings
    )
    if(FONTCONFIG_FOUND)
        target_include_directories(gg_gui PRIVATE ${FONTCONFIG_INCLUDE_DIRS})
        target_link_libraries(gg_gui PUBLIC ${FONTCONFIG_LIBRARIES})
    endif()

    set(GG_PLATFORM_SOURCES "")
    if(WIN32)
        set(GG_PLATFORM_SOURCES platform/windows/app.rc)
    endif()

    add_executable(GradeGoal src/main.c ${GG_PLATFORM_SOURCES})
    target_link_libraries(GradeGoal PRIVATE gg_gui gg_warnings)

    install(TARGETS GradeGoal DESTINATION bin)
    if(UNIX AND NOT APPLE)
        install(FILES platform/linux/gradegoal.desktop DESTINATION share/applications)
        install(FILES assets/icon/hicolor/16x16/apps/gradegoal.png DESTINATION share/icons/hicolor/16x16/apps)
        install(FILES assets/icon/hicolor/32x32/apps/gradegoal.png DESTINATION share/icons/hicolor/32x32/apps)
        install(FILES assets/icon/hicolor/48x48/apps/gradegoal.png DESTINATION share/icons/hicolor/48x48/apps)
        install(FILES assets/icon/hicolor/64x64/apps/gradegoal.png DESTINATION share/icons/hicolor/64x64/apps)
        install(FILES assets/icon/hicolor/128x128/apps/gradegoal.png DESTINATION share/icons/hicolor/128x128/apps)
        install(FILES assets/icon/hicolor/256x256/apps/gradegoal.png DESTINATION share/icons/hicolor/256x256/apps)
    endif()
endif()

enable_testing()

macro(gg_add_unit_test TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE gg_core unity gg_warnings)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

if(TARGET unity)
    gg_add_unit_test(test_scale_validator tests/unit/test_scale_validator.c)
    gg_add_unit_test(test_cgpa_calculator tests/unit/test_cgpa_calculator.c)
    gg_add_unit_test(test_target_solver tests/unit/test_target_solver.c)
    gg_add_unit_test(test_adversarial_m1 tests/unit/test_adversarial_m1.c)
    gg_add_unit_test(test_adversarial_m1_challenger2 tests/unit/test_adversarial_m1_challenger2.c)

    if(TARGET gg_persistence AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_sqlite_repositories.c")
        add_executable(test_sqlite_repositories tests/integration/test_sqlite_repositories.c)
        target_link_libraries(test_sqlite_repositories PRIVATE gg_persistence unity gg_warnings)
        add_test(NAME test_sqlite_repositories COMMAND test_sqlite_repositories)
    endif()

    if(TARGET gg_persistence AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_adversarial_m2_challenger2.c")
        add_executable(test_adversarial_m2_challenger2 tests/integration/test_adversarial_m2_challenger2.c)
        target_link_libraries(test_adversarial_m2_challenger2 PRIVATE gg_persistence unity gg_warnings)
        add_test(NAME test_adversarial_m2_challenger2 COMMAND test_adversarial_m2_challenger2)
    endif()

    if(TARGET gg_persistence AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_adversarial_m2.c")
        add_executable(test_adversarial_m2 tests/integration/test_adversarial_m2.c)
        target_link_libraries(test_adversarial_m2 PRIVATE gg_persistence unity gg_warnings)
        add_test(NAME test_adversarial_m2 COMMAND test_adversarial_m2)
    endif()

    if(TARGET gg_io AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_backup_manager.c")
        add_executable(test_backup_manager tests/unit/test_backup_manager.c)
        target_link_libraries(test_backup_manager PRIVATE gg_io gg_persistence unity gg_warnings)
        add_test(NAME test_backup_manager COMMAND test_backup_manager)
    endif()

    if(TARGET gg_io AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_xlsx_exporter.c")
        add_executable(test_xlsx_exporter tests/unit/test_xlsx_exporter.c)
        target_link_libraries(test_xlsx_exporter PRIVATE gg_io gg_persistence unity gg_warnings)
        add_test(NAME test_xlsx_exporter COMMAND test_xlsx_exporter)
    endif()

    if(TARGET gg_io AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_xlsx_importer.c")
        add_executable(test_xlsx_importer tests/unit/test_xlsx_importer.c)
        target_link_libraries(test_xlsx_importer PRIVATE gg_io gg_persistence unity gg_warnings)
        add_test(NAME test_xlsx_importer COMMAND test_xlsx_importer)
    endif()

    if(TARGET gg_io AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_pdf_exporter.c")
        add_executable(test_pdf_exporter tests/unit/test_pdf_exporter.c)
        target_link_libraries(test_pdf_exporter PRIVATE gg_io gg_persistence unity gg_warnings)
        add_test(NAME test_pdf_exporter COMMAND test_pdf_exporter)
    endif()
endif()

