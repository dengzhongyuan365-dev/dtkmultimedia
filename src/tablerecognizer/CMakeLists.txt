# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(BIN_NAME dtk${DTK_VERSION_MAJOR}tablerecognizer)

file(GLOB_RECURSE INSTALL_INCLUDE_FILES "../../include/dtktablerecognizer/*")
file(GLOB_RECURSE INCLUDE_FILES "./*.h")
file(GLOB_RECURSE SRCS "./*.h" "./*.cpp")

include_directories(../../include/dtktablerecognizer)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Concurrent)
find_package(PkgConfig REQUIRED)

# ===== onnxruntime 查找策略（三架构统一）=====
# 统一导出 imported target Ort::Runtime：
#  - 三架构（arm64/x86_64/loong64）优先经 pkg_check_modules(libonnxruntime) 查找；
#  - 通用 find_path/find_library 回退：按 ONNXRUNTIME_ROOT_DIR 定位预编译/源码编译产物
#    （不嵌入完整 ORT 源码构建）。
set(_ORT_FOUND FALSE)
pkg_check_modules(ORT IMPORTED_TARGET libonnxruntime)
if(ORT_FOUND)
    set(_ORT_FOUND TRUE)
    add_library(Ort::Runtime ALIAS PkgConfig::ORT)
endif()
if(NOT _ORT_FOUND)
    find_path(ORT_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        HINTS ENV ONNXRUNTIME_ROOT_DIR ${ONNXRUNTIME_ROOT_DIR}
        PATHS /usr /usr/local
        PATH_SUFFIXES include include/onnxruntime include/onnxruntime/core/session)
    find_library(ORT_LIBRARY
        NAMES onnxruntime
        HINTS ENV ONNXRUNTIME_ROOT_DIR ${ONNXRUNTIME_ROOT_DIR}
        PATHS /usr /usr/local
        PATH_SUFFIXES lib lib64)
    if(ORT_INCLUDE_DIR AND ORT_LIBRARY)
        set(_ORT_FOUND TRUE)
        add_library(Ort::Runtime SHARED IMPORTED)
        set_target_properties(Ort::Runtime PROPERTIES
            IMPORTED_LOCATION "${ORT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}")
    endif()
endif()
if(NOT _ORT_FOUND)
    message(FATAL_ERROR
        "dtk${DTK_VERSION_MAJOR}tablerecognizer: onnxruntime is mandatory and not found. "
        "Install libonnxruntime-dev, or set ONNXRUNTIME_ROOT_DIR to a prebuilt ORT install "
        "prefix; or pass -DDTK_ENABLE_TABLERECOGNIZER=OFF to skip this sub-library during "
        "an infra outage.")
endif()
mark_as_advanced(ORT_INCLUDE_DIR ORT_LIBRARY)

# opencv_mobile（降级/预处理）为 Phase 1 必需依赖。
pkg_check_modules(OpenCVMobile IMPORTED_TARGET opencv_mobile)
if(NOT OpenCVMobile_FOUND)
    message(FATAL_ERROR
        "dtk${DTK_VERSION_MAJOR}tablerecognizer: opencv_mobile is mandatory and not found. "
        "Install opencv-mobile dev package, or pass -DDTK_ENABLE_TABLERECOGNIZER=OFF to skip "
        "this sub-library.")
endif()

add_library(${BIN_NAME} SHARED
    ${INSTALL_INCLUDE_FILES}
    ${INCLUDE_FILES}
    ${SRCS}
)

set_target_properties(${BIN_NAME} PROPERTIES
    VERSION ${CMAKE_PROJECT_VERSION}
    SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR})

target_compile_definitions(${BIN_NAME} PRIVATE
    VERSION="${CMAKE_PROJECT_VERSION}"
    DTK_BUILD_TABLERECOGNIZER_LIB)

target_include_directories(${BIN_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../../include/dtktablerecognizer)

# 复用 libdtk6ocr 公开 API（DOcr），不修改其源码。
# dtk6ocr 目标未通过 INTERFACE_INCLUDE_DIRECTORIES 导出头文件路径，需显式添加。
target_include_directories(${BIN_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include/dtkocr)
target_link_libraries(${BIN_NAME}
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
    PRIVATE Ort::Runtime
    Qt${QT_VERSION_MAJOR}::Concurrent
    dtk${DTK_VERSION_MAJOR}ocr
    PkgConfig::OpenCVMobile)

# 模型随包分发路径（Debug 指向源码 models/，Release 指向 share 路径）。
set(ModelSaveSubDir models)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${BIN_NAME} PRIVATE
        TABLEREC_MODEL_DIR="${CMAKE_CURRENT_LIST_DIR}/${ModelSaveSubDir}/")
else()
    target_compile_definitions(${BIN_NAME} PRIVATE
        TABLEREC_MODEL_DIR="${CMAKE_INSTALL_PREFIX}/share/lib${BIN_NAME}/${ModelSaveSubDir}/")
    install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/${ModelSaveSubDir}
            DESTINATION share/lib${BIN_NAME})
endif()
