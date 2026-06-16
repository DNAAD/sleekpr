cmake_minimum_required(VERSION 3.24)

project(sleekpr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    src/core/templates/FieldPresetJson.cpp
    src/core/templates/FieldSchemaJson.cpp
    src/core/templates/PaperSpecJson.cpp
    src/core/templates/TemplateDocumentEditModel.cpp
    src/core/templates/TemplateDocumentFactory.cpp
    src/core/templates/TemplateDocumentJson.cpp
    src/core/templates/TemplateLibraryStore.cpp
    src/core/templates/TemplateDocumentRenderer.cpp
    src/core/templates/TemplateRenderContextBuilder.cpp
    src/infrastructure/preview/LabelPreviewImageRenderer.cpp
    src/infrastructure/preview/LabelPreviewService.cpp
    src/infrastructure/preview/PreviewLabelFactory.cpp
    src/infrastructure/preview/QrCodeMatrix.cpp
    src/infrastructure/preview/QrCodeMatrixRenderer.cpp
    src/infrastructure/printing/QtLabelPrintEngine.cpp
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
    src/http/LocalHttpRouter.cpp
)

target_link_libraries(sleekpr_http
    PUBLIC
        sleekpr_core
        Qt6::Core
        Qt6::Network
        Qt6::PrintSupport
)

add_executable(sleekpr
    src/app/PreviewWindow.cpp
    src/app/SettingsWindow.cpp
    src/app/TemplateDesignerWindow.cpp
    src/app/TemplateDragCoordinateMapper.cpp
    src/app/TemplateElementHitTester.cpp
    src/app/TemplatePreviewLabel.cpp
    src/app/WindowActivation.cpp
    src/app/main.cpp
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
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        # 发布前运行该目标会把 Qt 运行时和 platforms/qwindows*.dll 部署到 exe 旁边。
        add_custom_target(deploy_sleekpr
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:sleekpr>"
            DEPENDS sleekpr
            COMMENT "Deploying Qt runtime beside sleekpr.exe"
            VERBATIM
        )
    endif()
endif()
