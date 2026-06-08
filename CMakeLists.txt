cmake_minimum_required(VERSION 3.22)

project(hwinfo_detect VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING ON CACHE BOOL "" FORCE)

add_subdirectory(hwinfo)

# Shared library of common code
add_library(hwinfo_detect_common STATIC
    src/common.cpp
    src/logger.cpp
    src/config.cpp
)
target_compile_options(hwinfo_detect_common PRIVATE /utf-8)
target_link_libraries(hwinfo_detect_common PUBLIC lfreist-hwinfo::hwinfo)
if(WIN32)
    target_link_libraries(hwinfo_detect_common PUBLIC SetupAPI wbemuuid)
endif()

# Console executable
add_executable(hwinfo_detect src/main.cpp)
target_link_libraries(hwinfo_detect PRIVATE hwinfo_detect_common lfreist-hwinfo::hwinfo)

if(WIN32)
    target_link_libraries(hwinfo_detect PRIVATE SetupAPI wbemuuid Shell32)
endif()

add_custom_command(TARGET hwinfo_detect POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:hwinfo_detect>/config.json"
)
add_custom_command(TARGET hwinfo_detect POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:hwinfo_detect>/logs"
)

if(WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/app.ico ${CMAKE_CURRENT_BINARY_DIR}/app.ico COPYONLY)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/app.rc "1 ICON \"app.ico\"\n")
    add_executable(hwinfo_detect_gui WIN32 src/gui.cpp ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
    target_link_libraries(hwinfo_detect_gui PRIVATE hwinfo_detect_common lfreist-hwinfo::hwinfo)
    target_link_libraries(hwinfo_detect_gui PRIVATE comctl32)
    target_compile_options(hwinfo_detect_gui PRIVATE /utf-8)

    set(WEBVIEW2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webview2_sdk/extracted/build/native")
    target_include_directories(hwinfo_detect_gui PRIVATE "${WEBVIEW2_SDK_DIR}/include")
    target_link_libraries(hwinfo_detect_gui PRIVATE "${WEBVIEW2_SDK_DIR}/x64/WebView2LoaderStatic.lib")
    target_link_libraries(hwinfo_detect_gui PRIVATE Shell32 Ole32 OleAut32 User32 Version Winmm)

    add_custom_command(TARGET hwinfo_detect_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/web"
            "$<TARGET_FILE_DIR:hwinfo_detect_gui>/web"
    )
    add_custom_command(TARGET hwinfo_detect_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
            "$<TARGET_FILE_DIR:hwinfo_detect_gui>/config.json"
    )
    add_custom_command(TARGET hwinfo_detect_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:hwinfo_detect_gui>/logs"
    )
    add_custom_command(TARGET hwinfo_detect_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBVIEW2_SDK_DIR}/x64/WebView2Loader.dll"
            "$<TARGET_FILE_DIR:hwinfo_detect_gui>/WebView2Loader.dll"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/WebView2Runtime")
        add_custom_command(TARGET hwinfo_detect_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/WebView2Runtime"
                "$<TARGET_FILE_DIR:hwinfo_detect_gui>/WebView2Runtime"
        )
    endif()
endif()


