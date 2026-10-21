cmake_minimum_required(VERSION 3.24)

if(CMAKE_HOST_WIN32)
    # 配置和编译使用同一语言偏好；缺少英文语言包时仍允许 MSVC 回退到本机语言。
    set(ENV{VSLANG} "1033")
endif()

project(sleekpr VERSION 0.1.0 LANGUAGES CXX)

if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    # 本机可能只安装中文语言包，不能假定 VSLANG 一定能切成英文。
    # Qt/MSVC 的 /utf-8 输出必须按 UTF-8 读取，不能沿用被本机代码页误解码的缓存前缀。
    set(_sleekpr_probe_dir "${CMAKE_BINARY_DIR}/CMakeFiles/sleekpr-msvc-deps")
    file(MAKE_DIRECTORY "${_sleekpr_probe_dir}")
    file(WRITE "${_sleekpr_probe_dir}/sleekpr_msvc_probe.h" "#pragma once\n")
    file(WRITE "${_sleekpr_probe_dir}/probe.cpp" "#include \"sleekpr_msvc_probe.h\"\n")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" /nologo /utf-8 /Zs /showIncludes /TP "${_sleekpr_probe_dir}/probe.cpp"
        RESULT_VARIABLE _sleekpr_probe_result
        OUTPUT_VARIABLE _sleekpr_probe_output
        ERROR_VARIABLE _sleekpr_probe_error
        ENCODING UTF-8
    )
    string(REGEX MATCH "([^\r\n]*:)[ \t]+[^\r\n]*sleekpr_msvc_probe\\.h"
        _sleekpr_include_line "${_sleekpr_probe_output}${_sleekpr_probe_error}")
    if(NOT _sleekpr_probe_result EQUAL 0 OR NOT _sleekpr_include_line)
        message(FATAL_ERROR "无法探测 MSVC 头文件依赖前缀：${_sleekpr_probe_output}${_sleekpr_probe_error}")
    endif()
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_1} ")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
    set(_sleekpr_compiler_launcher "${CMAKE_COMMAND}" -E env "VSLANG=1033" --)
    if(CMAKE_CXX_COMPILER_LAUNCHER)
        list(APPEND _sleekpr_compiler_launcher ${CMAKE_CXX_COMPILER_LAUNCHER})
    endif()
    set(CMAKE_CXX_COMPILER_LAUNCHER "${_sleekpr_compiler_launcher}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SLEEKPR_ENABLE_RELEASE_ORCHESTRATOR "Enable the release packaging orchestrator target" ON)
option(SLEEKPR_ENABLE_TABLE_DESIGNER "启用延后的复杂表格设计器" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Gui Widgets PrintSupport Test)
qt_standard_project_setup()

if(CMAKE_CONFIGURATION_TYPES)
    set(SLEEKPR_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>")
    set(_sleekpr_metadata_configurations ${CMAKE_CONFIGURATION_TYPES})
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建模式" FORCE)
    endif()
    set(SLEEKPR_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(_sleekpr_metadata_configurations ${CMAKE_BUILD_TYPE})
endif()

foreach(configuration IN LISTS _sleekpr_metadata_configurations)
    if(CMAKE_CONFIGURATION_TYPES)
        set(initial_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${configuration}/sleekpr/BuildVersion.h")
    else()
        set(initial_header "${SLEEKPR_GENERATED_INCLUDE_DIR}/sleekpr/BuildVersion.h")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_HEADER=${initial_header}"
        "-DVERSION=${PROJECT_VERSION}" "-DCONFIGURATION=${configuration}"
        "-DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/packaging/BuildVersion.h.in"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake" COMMAND_ERROR_IS_FATAL ANY)
endforeach()

# 每次构建探测，但 configure_file 只在内容变化时更新头文件，不把时钟写进二进制。
add_custom_target(sleekpr_build_metadata ALL
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DOUTPUT_HEADER=${SLEEKPR_GENERATED_INCLUDE_DIR}/sleekpr/BuildVersion.h"
        "-DVERSION=${PROJECT_VERSION}" "-DCONFIGURATION=$<CONFIG>"
        "-DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/packaging/BuildVersion.h.in"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake"
    BYPRODUCTS "${SLEEKPR_GENERATED_INCLUDE_DIR}/sleekpr/BuildVersion.h"
    VERBATIM)

add_library(sleekpr_core
    src/core/labels/LabelRenderPlanner.cpp
    src/core/labels/TemplateOverrideKey.cpp
    src/core/native/NativeLabelDrawingPlanner.cpp
    src/core/native/NativePlanJsonSerializer.cpp
    src/core/native/SilverNativeLabelDrawingPlanner.cpp
    src/core/printing/LabelPaperSize.cpp
    src/core/printing/PrintUnitConverter.cpp
    src/core/printing/PrintCopyPlanner.cpp
    src/core/printing/PreviewResourceBudget.cpp
    src/core/security/AllowedOriginParser.cpp
    src/core/security/LocalCorsPolicy.cpp
    src/core/security/PrivateNetworkAccessPolicy.cpp
    src/infrastructure/settings/FileSettingsStore.cpp
    src/core/settings/PrinterSelectionResolver.cpp
    src/core/settings/PrintClientSettingsJson.cpp
    src/core/settings/SettingsEditModel.cpp
    src/core/settings/SettingsChangeSet.cpp
    src/core/settings/TemplateElementCatalog.cpp
    src/core/templates/DeviceProfileResolver.cpp
    src/core/templates/DefaultPaperSpecs.cpp
    src/core/templates/FieldPresetJson.cpp
    src/core/templates/FieldPresetStore.cpp
    src/core/templates/FieldSchemaJson.cpp
    src/core/templates/PaperSpecJson.cpp
    src/core/templates/PaperSpecStore.cpp
    src/core/templates/TableElementJson.cpp
    src/core/templates/TableElementLayout.cpp
    src/core/templates/TableElementRenderer.cpp
    src/core/templates/TemplateDocumentEditModel.cpp
    src/core/templates/TemplateSelectionEditModel.cpp
    src/core/templates/TemplateDocumentFactory.cpp
    src/core/templates/TemplateDocumentJson.cpp
    src/core/templates/TemplateLibraryStore.cpp
    src/core/templates/TemplateDocumentRenderer.cpp
    src/core/templates/TemplateDocumentValidator.cpp
    src/core/templates/TemplateInputSchema.cpp
    src/core/templates/TemplateRenderContextBuilder.cpp
    src/infrastructure/preview/LabelPreviewImageRenderer.cpp
    src/infrastructure/preview/PreviewExecutor.cpp
    src/infrastructure/preview/TemplatePreviewJob.cpp
    src/infrastructure/preview/LabelPreviewService.cpp
    src/infrastructure/preview/PreviewLabelFactory.cpp
    src/infrastructure/preview/QrCodeMatrix.cpp
    src/infrastructure/preview/QrCodeMatrixRenderer.cpp
    src/infrastructure/printing/QrModulePixelLayout.cpp
    src/infrastructure/printing/QtLabelPrintEngine.cpp
    src/infrastructure/printing/TemplatePrintService.cpp
    src/infrastructure/settings/SettingsRepository.cpp
    src/infrastructure/diagnostics/ClientPerformanceMetrics.cpp
    src/infrastructure/rendering/TextAutoFitSizer.cpp
)

target_include_directories(sleekpr_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sleekpr_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::PrintSupport
)

add_library(sleekpr_http
    src/http/LocalHttpServer.cpp
    src/http/LocalHttpRouteSupport.cpp
    src/http/LocalHttpRouter.cpp
    src/http/PreviewRoutes.cpp
    src/http/PreviewResponseBudget.cpp
    src/http/PrintRoutes.cpp
    src/http/SettingsRoutes.cpp
    src/http/TemplateRequestPlanner.cpp
    src/http/TemplateRoutes.cpp
)

target_link_libraries(sleekpr_http
    PUBLIC
        sleekpr_core
        Qt6::Core
        Qt6::Network
        Qt6::PrintSupport
)

add_executable(sleekpr
    resources/sleekpr-ui.qrc
    src/app/AppTheme.cpp
    src/app/TableAdvancedEditorPanel.cpp
    include/sleekpr/app/TableAdvancedEditorPanel.h
    src/app/TableColumnEditorPanel.cpp
    include/sleekpr/app/TableColumnEditorPanel.h
    src/app/TableColumnTextCodec.cpp
    include/sleekpr/app/TableColumnTextCodec.h
    src/app/TemplateDesignerCommand.cpp
    src/app/TemplateCanvasEditor.cpp
    src/app/FieldPresetManagerWindow.cpp
    src/app/FieldBindingSelector.cpp
    src/app/ElementBindingEditor.cpp
    include/sleekpr/app/ElementBindingEditor.h
    src/app/TemplateElementBindings.cpp
    src/app/TemplateFieldNames.cpp
    include/sleekpr/app/FieldBindingSelector.h
    src/app/PaperSpecManagerWindow.cpp
    src/app/PrintWorkbenchWindow.cpp
    src/app/PrintWorkbenchPreview.cpp
    src/app/PrintPreviewPanel.cpp
    src/app/PrintFieldEditorPanel.cpp
    include/sleekpr/app/PrintFieldEditorPanel.h
    src/app/PrintPreflightPanel.cpp
    include/sleekpr/app/PrintPreflightPanel.h
    src/app/SettingsWindow.cpp
    src/app/BuildInfo.cpp
    src/app/BuildInfoDialog.cpp
    src/app/SettingsWindowRefresh.cpp
    src/app/SettingsWindowPreview.cpp
    src/app/LatestPreviewQueue.cpp
    include/sleekpr/app/LatestPreviewQueue.h
    src/app/SettingsEditSession.cpp
    src/app/SettingsPersistenceNotice.cpp
    src/app/TemplateDesignerPersistence.cpp
    src/app/TemplateDesignerPreview.cpp
    src/app/TemplateDesignerFactory.cpp
    src/app/TemplateInspectorPanel.cpp
    include/sleekpr/app/TemplateInspectorPanel.h
    src/app/TemplateDesignerPresenter.cpp
    src/app/TemplateDesignerState.cpp
    src/app/TemplateDesignerWindow.cpp
    src/app/TemplateDragCoordinateMapper.cpp
    src/app/TemplateElementHitTester.cpp
    src/app/TemplateLayerPanel.cpp
    src/app/TemplateLibraryPanel.cpp
    src/app/TemplatePreviewLabel.cpp
    include/sleekpr/app/TemplateTablePaginationOverlay.h
    src/app/TemplateTablePaginationOverlayPlanner.cpp
    include/sleekpr/app/TemplateTablePaginationOverlayPlanner.h
    src/app/TemplateTableCanvasEditor.cpp
    include/sleekpr/app/TemplateTableCanvasEditor.h
    src/app/TemplateWorkspacePanel.cpp
    src/app/TemplateWorkspaceToolbar.cpp
    src/app/WindowActivation.cpp
    src/app/main.cpp
)

# 桌面客户端使用 Windows GUI 子系统，双击 exe 时不弹出控制台黑框。
set_target_properties(sleekpr PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(sleekpr PROPERTIES AUTORCC ON)
add_dependencies(sleekpr sleekpr_build_metadata)
target_include_directories(sleekpr PRIVATE "${SLEEKPR_GENERATED_INCLUDE_DIR}")
if(WIN32)
    enable_language(RC)
    configure_file(packaging/WindowsVersion.rc.in "${CMAKE_CURRENT_BINARY_DIR}/generated/WindowsVersion.rc" @ONLY)
    target_sources(sleekpr PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/WindowsVersion.rc")
endif()
target_compile_definitions(sleekpr PRIVATE
    SLEEKPR_ENABLE_TABLE_DESIGNER=$<BOOL:${SLEEKPR_ENABLE_TABLE_DESIGNER}>
)

target_link_libraries(sleekpr
    PRIVATE
        sleekpr_core
        sleekpr_http
        Qt6::Core
        Qt6::Widgets
        Qt6::PrintSupport
)

enable_testing()
add_subdirectory(tests)

install(TARGETS sleekpr
    RUNTIME DESTINATION .
)

if(WIN32)
    get_target_property(QT_CORE_LOCATION Qt6::Core LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_CORE_LOCATION}" DIRECTORY)
    get_filename_component(SLEEKPR_QT_PREFIX "${QT_BIN_DIR}/.." ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        # 发布前运行该目标，会把 Qt 运行时和 platforms/qwindows*.dll 部署到当前构建产物旁边。
        add_custom_target(deploy_sleekpr
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:sleekpr>"
            DEPENDS sleekpr
            COMMENT "部署 Qt 运行时到当前构建目录"
            VERBATIM
        )

        if(SLEEKPR_ENABLE_RELEASE_ORCHESTRATOR)
            # 所有正式发布入口走同一验收脚本，不再在 CMake 中无条件清空目录或先发布后测试。
            find_program(SLEEKPR_PACKAGE_POWERSHELL NAMES powershell.exe powershell pwsh.exe pwsh REQUIRED)
            add_custom_target(package_sleekpr_release
                COMMAND "${SLEEKPR_PACKAGE_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/package-portable.ps1" -QtRoot "${SLEEKPR_QT_PREFIX}"
                COMMENT "构建、验收并安全发布 Release 绿色包"
                VERBATIM
            )
        endif()

        if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE STREQUAL "Release")
            foreach(legacy_target IN ITEMS deploy_sleekpr_release_current package_sleekpr_release_current)
                add_custom_target(${legacy_target}
                    COMMAND "${CMAKE_COMMAND}" -E echo "旧内部发布入口已禁用，请运行 package-portable.ps1 完成测试和暂存验收。"
                    COMMAND "${CMAKE_COMMAND}" -E false VERBATIM)
            endforeach()
        endif()
    endif()
endif()
