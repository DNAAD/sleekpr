cmake_minimum_required(VERSION 3.24)

project(sleekpr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SLEEKPR_ENABLE_RELEASE_ORCHESTRATOR "Enable the release packaging orchestrator target" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Gui Widgets PrintSupport Test)
qt_standard_project_setup()

add_library(sleekpr_core
    src/core/labels/LabelRenderPlanner.cpp
    src/core/labels/TemplateOverrideKey.cpp
    src/core/native/NativeLabelDrawingPlanner.cpp
    src/core/native/NativePlanJsonSerializer.cpp
    src/core/native/SilverNativeLabelDrawingPlanner.cpp
    src/core/printing/LabelPaperSize.cpp
    src/core/printing/PrintUnitConverter.cpp
    src/core/security/AllowedOriginParser.cpp
    src/core/security/LocalCorsPolicy.cpp
    src/core/security/PrivateNetworkAccessPolicy.cpp
    src/core/settings/FileSettingsStore.cpp
    src/core/settings/PrinterSelectionResolver.cpp
    src/core/settings/PrintClientSettingsJson.cpp
    src/core/settings/SettingsEditModel.cpp
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
    src/core/templates/TemplateDocumentFactory.cpp
    src/core/templates/TemplateDocumentJson.cpp
    src/core/templates/TemplateLibraryStore.cpp
    src/core/templates/TemplateDocumentRenderer.cpp
    src/core/templates/TemplateDocumentValidator.cpp
    src/core/templates/TemplateRenderContextBuilder.cpp
    src/infrastructure/preview/LabelPreviewImageRenderer.cpp
    src/infrastructure/preview/LabelPreviewService.cpp
    src/infrastructure/preview/PreviewLabelFactory.cpp
    src/infrastructure/preview/QrCodeMatrix.cpp
    src/infrastructure/preview/QrCodeMatrixRenderer.cpp
    src/infrastructure/printing/QrModulePixelLayout.cpp
    src/infrastructure/printing/QtLabelPrintEngine.cpp
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
    src/app/AppTheme.cpp
    src/app/TableAdvancedEditorPanel.cpp
    include/sleekpr/app/TableAdvancedEditorPanel.h
    src/app/TableColumnEditorPanel.cpp
    include/sleekpr/app/TableColumnEditorPanel.h
    src/app/TableColumnTextCodec.cpp
    include/sleekpr/app/TableColumnTextCodec.h
    src/app/TemplateDesignerCommand.cpp
    src/app/FieldPresetManagerWindow.cpp
    src/app/PaperSpecManagerWindow.cpp
    src/app/SettingsWindow.cpp
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
    src/app/TemplateWorkspacePanel.cpp
    src/app/TemplateWorkspaceToolbar.cpp
    src/app/WindowActivation.cpp
    src/app/main.cpp
)

# 桌面客户端使用 Windows GUI 子系统，双击 exe 时不弹出控制台黑框。
set_target_properties(sleekpr PROPERTIES WIN32_EXECUTABLE TRUE)

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
            COMMENT "Deploying Qt runtime beside sleekpr.exe"
            VERBATIM
        )

        set(SLEEKPR_RELEASE_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build-release")
        set(SLEEKPR_RELEASE_DIST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dist")
        set(SLEEKPR_RELEASE_STAGE_DIR "${SLEEKPR_RELEASE_DIST_ROOT}/sleekpr-release")
        set(SLEEKPR_RELEASE_ARCHIVE "${SLEEKPR_RELEASE_DIST_ROOT}/sleekpr-${PROJECT_VERSION}-windows-x64.zip")

        set(SLEEKPR_RELEASE_CONFIGURE_ARGS
            -S "${CMAKE_CURRENT_SOURCE_DIR}"
            -B "${SLEEKPR_RELEASE_BUILD_DIR}"
            -G "${CMAKE_GENERATOR}"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_PREFIX_PATH=${SLEEKPR_QT_PREFIX}"
            "-DSLEEKPR_ENABLE_RELEASE_ORCHESTRATOR=OFF"
        )
        if(CMAKE_GENERATOR_PLATFORM)
            list(APPEND SLEEKPR_RELEASE_CONFIGURE_ARGS -A "${CMAKE_GENERATOR_PLATFORM}")
        endif()
        if(CMAKE_GENERATOR_TOOLSET)
            list(APPEND SLEEKPR_RELEASE_CONFIGURE_ARGS -T "${CMAKE_GENERATOR_TOOLSET}")
        endif()

        if(SLEEKPR_ENABLE_RELEASE_ORCHESTRATOR)
            # 外层目标只负责编排独立 Release 构建，真正打包动作由 build-release 中的内部目标完成。
            add_custom_target(package_sleekpr_release
                COMMAND "${CMAKE_COMMAND}" -E rm -rf "${SLEEKPR_RELEASE_BUILD_DIR}"
                COMMAND "${CMAKE_COMMAND}" ${SLEEKPR_RELEASE_CONFIGURE_ARGS}
                # 先构建默认目标，确保 Release 测试程序也会生成，便于后续 CTest 验证。
                COMMAND "${CMAKE_COMMAND}" --build "${SLEEKPR_RELEASE_BUILD_DIR}" --config Release
                COMMAND "${CMAKE_COMMAND}" --build "${SLEEKPR_RELEASE_BUILD_DIR}" --config Release --target package_sleekpr_release_current
                COMMENT "Building and packaging sleekpr Release archive"
                VERBATIM
            )
        endif()

        if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE STREQUAL "Release")
            # 内部目标仅在 Release 构建目录中打包当前产物，避免把 Debug exe 放进正式包。
            add_custom_target(deploy_sleekpr_release_current
                COMMAND "${CMAKE_COMMAND}" -E rm -rf "${SLEEKPR_RELEASE_STAGE_DIR}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${SLEEKPR_RELEASE_STAGE_DIR}"
                COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:sleekpr>" "${SLEEKPR_RELEASE_STAGE_DIR}/sleekpr.exe"
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations --dir "${SLEEKPR_RELEASE_STAGE_DIR}" "${SLEEKPR_RELEASE_STAGE_DIR}/sleekpr.exe"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${SLEEKPR_RELEASE_STAGE_DIR}/LICENSE"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/packaging/README-PORTABLE.txt" "${SLEEKPR_RELEASE_STAGE_DIR}/README-PORTABLE.txt"
                DEPENDS sleekpr
                COMMENT "Deploying sleekpr Release runtime into dist/sleekpr-release"
                VERBATIM
            )

            add_custom_target(package_sleekpr_release_current
                COMMAND "${CMAKE_COMMAND}" -E rm -f "${SLEEKPR_RELEASE_ARCHIVE}"
                COMMAND "${CMAKE_COMMAND}" -E tar cf "${SLEEKPR_RELEASE_ARCHIVE}" --format=zip "sleekpr-release"
                WORKING_DIRECTORY "${SLEEKPR_RELEASE_DIST_ROOT}"
                DEPENDS deploy_sleekpr_release_current
                COMMENT "Creating sleekpr Release zip package"
                VERBATIM
            )
        endif()
    endif()
endif()
