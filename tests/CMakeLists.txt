add_executable(sleekpr_core_tests
    tst_core.cpp
)

target_link_libraries(sleekpr_core_tests
    PRIVATE
        sleekpr_core
        Qt6::Test
)

add_test(NAME sleekpr_core_tests COMMAND sleekpr_core_tests)

if(WIN32)
    function(configure_qt_test_runtime target_name)
        if(TARGET ${target_name})
            add_dependencies(${target_name} sleekpr_build_metadata)
            target_include_directories(${target_name} PRIVATE "${SLEEKPR_GENERATED_INCLUDE_DIR}")
        endif()
        # CTest 独立启动测试进程时不会自动继承 Qt bin 路径，所有 Qt 测试都需要补齐 DLL 搜索路径。
        set_tests_properties(${target_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endfunction()
else()
    function(configure_qt_test_runtime target_name)
        if(TARGET ${target_name})
            add_dependencies(${target_name} sleekpr_build_metadata)
            target_include_directories(${target_name} PRIVATE "${SLEEKPR_GENERATED_INCLUDE_DIR}")
        endif()
        # 非 Windows 平台不需要额外设置 PATH，保留同名函数让每个测试目标的配置方式一致。
    endfunction()
endif()

configure_qt_test_runtime(sleekpr_core_tests)

foreach(case_name IN ITEMS reportsBuildModeAndRevision diagnosticDoesNotTouchUserSettings marksMissingGitUnknown distinguishesDocsFromSourceDirty updatesWithoutManualReconfigure)
    add_test(NAME sleekpr_build_info_${case_name} COMMAND "${CMAKE_COMMAND}"
        "-DCASE=${case_name}" "-DAPP_EXE=$<TARGET_FILE:sleekpr>" "-DEXPECTED_CONFIG=$<CONFIG>"
        "-DEXPECTED_VERSION=${PROJECT_VERSION}" "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DTEST_ROOT=${CMAKE_BINARY_DIR}/build-info-tests" "-DGENERATOR_SCRIPT=${CMAKE_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake"
        "-DBUILD_TEMPLATE=${CMAKE_SOURCE_DIR}/packaging/BuildVersion.h.in" "-DNINJA_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_build_info.cmake")
    configure_qt_test_runtime(sleekpr_build_info_${case_name})
endforeach()

if(WIN32)
    find_program(SLEEKPR_POWERSHELL NAMES powershell.exe powershell pwsh.exe pwsh REQUIRED)
    add_test(NAME sleekpr_build_identity_test COMMAND "${SLEEKPR_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/check_build_identity.ps1" -RepoRoot "${CMAKE_SOURCE_DIR}"
        -TestRoot "${CMAKE_BINARY_DIR}/build-identity-tests" -ExecutablePath "$<TARGET_FILE:sleekpr>")
    configure_qt_test_runtime(sleekpr_build_identity_test)
    add_test(NAME sleekpr_portable_package_test COMMAND "${SLEEKPR_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/check_portable_package.ps1" -RepoRoot "${CMAKE_SOURCE_DIR}"
        -TestRoot "${CMAKE_BINARY_DIR}/portable-package-tests")
endif()

add_executable(sleekpr_settings_change_set_tests tst_settings_change_set.cpp)
target_link_libraries(sleekpr_settings_change_set_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_settings_change_set_tests COMMAND sleekpr_settings_change_set_tests)
configure_qt_test_runtime(sleekpr_settings_change_set_tests)

add_executable(sleekpr_client_performance_tests tst_client_performance_metrics.cpp)
target_link_libraries(sleekpr_client_performance_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_client_performance_tests COMMAND sleekpr_client_performance_tests)
configure_qt_test_runtime(sleekpr_client_performance_tests)

add_executable(sleekpr_preview_budget_tests tst_preview_resource_budget.cpp)
target_link_libraries(sleekpr_preview_budget_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_preview_budget_tests COMMAND sleekpr_preview_budget_tests)
configure_qt_test_runtime(sleekpr_preview_budget_tests)

add_executable(sleekpr_preview_image_tests tst_preview_image_renderer.cpp)
target_link_libraries(sleekpr_preview_image_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_preview_image_tests COMMAND sleekpr_preview_image_tests)
configure_qt_test_runtime(sleekpr_preview_image_tests)

add_executable(sleekpr_preview_planning_tests tst_preview_planning_budget.cpp)
target_link_libraries(sleekpr_preview_planning_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_preview_planning_tests COMMAND sleekpr_preview_planning_tests)
configure_qt_test_runtime(sleekpr_preview_planning_tests)

add_executable(sleekpr_settings_repository_tests tst_settings_repository.cpp)
target_link_libraries(sleekpr_settings_repository_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_settings_repository_tests COMMAND sleekpr_settings_repository_tests)
configure_qt_test_runtime(sleekpr_settings_repository_tests)

add_executable(sleekpr_settings_edit_session_tests tst_settings_edit_session.cpp ../src/app/SettingsEditSession.cpp)
target_link_libraries(sleekpr_settings_edit_session_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_settings_edit_session_tests COMMAND sleekpr_settings_edit_session_tests)
configure_qt_test_runtime(sleekpr_settings_edit_session_tests)

add_executable(sleekpr_preview_queue_tests tst_latest_preview_queue.cpp
    ../src/app/LatestPreviewQueue.cpp ../include/sleekpr/app/LatestPreviewQueue.h)
target_link_libraries(sleekpr_preview_queue_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_preview_queue_tests COMMAND sleekpr_preview_queue_tests)
configure_qt_test_runtime(sleekpr_preview_queue_tests)

add_executable(sleekpr_template_preview_job_tests tst_template_preview_job.cpp
    ../src/app/LatestPreviewQueue.cpp ../include/sleekpr/app/LatestPreviewQueue.h)
target_link_libraries(sleekpr_template_preview_job_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_template_preview_job_tests COMMAND sleekpr_template_preview_job_tests)
configure_qt_test_runtime(sleekpr_template_preview_job_tests)

add_executable(sleekpr_app_tests
    tst_app.cpp
    ../resources/sleekpr-ui.qrc
    ../src/app/AppTheme.cpp
    ../src/app/TableAdvancedEditorPanel.cpp
    ../include/sleekpr/app/TableAdvancedEditorPanel.h
    ../src/app/TableColumnEditorPanel.cpp
    ../include/sleekpr/app/TableColumnEditorPanel.h
    ../src/app/TableColumnTextCodec.cpp
    ../include/sleekpr/app/TableColumnTextCodec.h
    ../src/app/TemplateDesignerCommand.cpp
    ../src/app/TemplateCanvasEditor.cpp
    ../src/app/FieldPresetManagerWindow.cpp
    ../src/app/FieldBindingSelector.cpp
    ../src/app/ElementBindingEditor.cpp
    ../include/sleekpr/app/ElementBindingEditor.h
    ../src/app/TemplateElementBindings.cpp
    ../src/app/TemplateFieldNames.cpp
    ../include/sleekpr/app/FieldBindingSelector.h
    ../src/app/PaperSpecManagerWindow.cpp
    ../src/app/SettingsWindow.cpp
    ../src/app/BuildInfo.cpp
    ../src/app/BuildInfoDialog.cpp
    ../src/app/SettingsWindowRefresh.cpp
    ../src/app/SettingsWindowPreview.cpp
    ../src/app/LatestPreviewQueue.cpp
    ../include/sleekpr/app/LatestPreviewQueue.h
    ../src/app/SettingsEditSession.cpp
    ../src/app/SettingsPersistenceNotice.cpp
    ../src/app/TemplateDesignerPersistence.cpp
    ../src/app/TemplateDesignerPreview.cpp
    ../src/app/TemplateDesignerFactory.cpp
    ../src/app/TemplateInspectorPanel.cpp
    ../include/sleekpr/app/TemplateInspectorPanel.h
    ../src/app/TemplateDesignerPresenter.cpp
    ../src/app/TemplateDesignerState.cpp
    ../src/app/TemplateDesignerWindow.cpp
    ../src/app/TemplateDragCoordinateMapper.cpp
    ../src/app/TemplateElementHitTester.cpp
    ../src/app/TemplateLayerPanel.cpp
    ../src/app/TemplateLibraryPanel.cpp
    ../src/app/TemplatePreviewLabel.cpp
    ../include/sleekpr/app/TemplateTablePaginationOverlay.h
    ../src/app/TemplateTablePaginationOverlayPlanner.cpp
    ../include/sleekpr/app/TemplateTablePaginationOverlayPlanner.h
    ../src/app/TemplateTableCanvasEditor.cpp
    ../include/sleekpr/app/TemplateTableCanvasEditor.h
    ../src/app/TemplateWorkspacePanel.cpp
    ../src/app/TemplateWorkspaceToolbar.cpp
    ../src/app/WindowActivation.cpp
)

target_link_libraries(sleekpr_app_tests
    PRIVATE
        sleekpr_core
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Test
)

add_test(NAME sleekpr_app_tests COMMAND sleekpr_app_tests)
set_target_properties(sleekpr_app_tests PROPERTIES AUTORCC ON)
# 延后模块继续保留完整回归覆盖，实际标签版本另用默认关闭表格能力的目标验证。
target_compile_definitions(sleekpr_app_tests PRIVATE SLEEKPR_ENABLE_TABLE_DESIGNER=1)
configure_qt_test_runtime(sleekpr_app_tests)

# 标签版本单独验证实际发布界面，表格编辑器的既有测试继续保留供后续恢复使用。
get_target_property(SLEEKPR_LABEL_APP_SOURCES sleekpr SOURCES)
list(REMOVE_ITEM SLEEKPR_LABEL_APP_SOURCES src/app/main.cpp)
set(SLEEKPR_LABEL_APP_TEST_SOURCES)
foreach(source IN LISTS SLEEKPR_LABEL_APP_SOURCES)
    if(IS_ABSOLUTE "${source}")
        list(APPEND SLEEKPR_LABEL_APP_TEST_SOURCES "${source}")
    else()
        list(APPEND SLEEKPR_LABEL_APP_TEST_SOURCES "../${source}")
    endif()
endforeach()
add_executable(sleekpr_label_app_tests
    tst_label_designer.cpp
    ${SLEEKPR_LABEL_APP_TEST_SOURCES}
)
set_target_properties(sleekpr_label_app_tests PROPERTIES AUTORCC ON)
target_link_libraries(sleekpr_label_app_tests PRIVATE
    sleekpr_core Qt6::Widgets Qt6::PrintSupport Qt6::Test
)
target_compile_definitions(sleekpr_label_app_tests PRIVATE SLEEKPR_ENABLE_TABLE_DESIGNER=0)
add_test(NAME sleekpr_label_app_tests COMMAND sleekpr_label_app_tests)
configure_qt_test_runtime(sleekpr_label_app_tests)

add_executable(sleekpr_selection_tests tst_selection.cpp)
target_link_libraries(sleekpr_selection_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_selection_tests COMMAND sleekpr_selection_tests)
configure_qt_test_runtime(sleekpr_selection_tests)

add_executable(sleekpr_canvas_tests tst_canvas_interaction.cpp ../src/app/TemplatePreviewLabel.cpp)
target_link_libraries(sleekpr_canvas_tests PRIVATE sleekpr_core Qt6::Widgets Qt6::Test)
add_test(NAME sleekpr_canvas_tests COMMAND sleekpr_canvas_tests)
configure_qt_test_runtime(sleekpr_canvas_tests)

add_executable(sleekpr_template_print_tests tst_template_print_service.cpp)
target_link_libraries(sleekpr_template_print_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_template_print_tests COMMAND sleekpr_template_print_tests)
configure_qt_test_runtime(sleekpr_template_print_tests)

add_executable(sleekpr_template_input_tests tst_template_input_schema.cpp)
target_link_libraries(sleekpr_template_input_tests PRIVATE sleekpr_core Qt6::Test)
add_test(NAME sleekpr_template_input_tests COMMAND sleekpr_template_input_tests)
configure_qt_test_runtime(sleekpr_template_input_tests)

add_executable(sleekpr_field_editor_tests
    tst_print_field_editor.cpp
    ../src/app/PrintFieldEditorPanel.cpp
    ../src/app/TemplateFieldNames.cpp
    ../include/sleekpr/app/PrintFieldEditorPanel.h
    ../src/app/AppTheme.cpp
    ../resources/sleekpr-ui.qrc
)
set_target_properties(sleekpr_field_editor_tests PROPERTIES AUTORCC ON)
target_link_libraries(sleekpr_field_editor_tests PRIVATE sleekpr_core Qt6::Widgets Qt6::Test)
add_test(NAME sleekpr_field_editor_tests COMMAND sleekpr_field_editor_tests)
configure_qt_test_runtime(sleekpr_field_editor_tests)

add_executable(sleekpr_workbench_tests
    tst_print_workbench.cpp
    ../src/app/PrintWorkbenchWindow.cpp
    ../src/app/BuildInfo.cpp
    ../src/app/BuildInfoDialog.cpp
    ../src/app/PrintWorkbenchPreview.cpp
    ../src/app/LatestPreviewQueue.cpp
    ../include/sleekpr/app/LatestPreviewQueue.h
    ../src/app/PrintPreviewPanel.cpp
    ../src/app/PrintFieldEditorPanel.cpp
    ../src/app/TemplateFieldNames.cpp
    ../include/sleekpr/app/PrintFieldEditorPanel.h
    ../src/app/PrintPreflightPanel.cpp
    ../include/sleekpr/app/PrintPreflightPanel.h
    ../src/app/AppTheme.cpp
    ../resources/sleekpr-ui.qrc
)
set_target_properties(sleekpr_workbench_tests PROPERTIES AUTORCC ON)
target_link_libraries(sleekpr_workbench_tests PRIVATE sleekpr_core Qt6::Widgets Qt6::PrintSupport Qt6::Test)
add_test(NAME sleekpr_workbench_tests COMMAND sleekpr_workbench_tests)
configure_qt_test_runtime(sleekpr_workbench_tests)

add_executable(sleekpr_preflight_panel_tests
    tst_print_preflight_panel.cpp
    ../src/app/PrintPreflightPanel.cpp
    ../include/sleekpr/app/PrintPreflightPanel.h
    ../src/app/AppTheme.cpp
    ../resources/sleekpr-ui.qrc
)
set_target_properties(sleekpr_preflight_panel_tests PROPERTIES AUTORCC ON)
target_link_libraries(sleekpr_preflight_panel_tests PRIVATE sleekpr_core Qt6::Widgets Qt6::Test)
add_test(NAME sleekpr_preflight_panel_tests COMMAND sleekpr_preflight_panel_tests)
configure_qt_test_runtime(sleekpr_preflight_panel_tests)

add_executable(sleekpr_print_preview_tests
    tst_print_preview.cpp
    ../src/app/LatestPreviewQueue.cpp
    ../include/sleekpr/app/LatestPreviewQueue.h
    ../src/app/PrintPreviewPanel.cpp
    ../src/app/AppTheme.cpp
    ../resources/sleekpr-ui.qrc
)
set_target_properties(sleekpr_print_preview_tests PROPERTIES AUTORCC ON)
target_link_libraries(sleekpr_print_preview_tests PRIVATE sleekpr_core Qt6::Widgets Qt6::Test)
add_test(NAME sleekpr_print_preview_tests COMMAND sleekpr_print_preview_tests)
configure_qt_test_runtime(sleekpr_print_preview_tests)

add_executable(sleekpr_http_tests
    tst_http.cpp
)

target_link_libraries(sleekpr_http_tests
    PRIVATE
        sleekpr_core
        sleekpr_http
        Qt6::Test
)

add_test(NAME sleekpr_http_tests COMMAND sleekpr_http_tests)
configure_qt_test_runtime(sleekpr_http_tests)

# 直接测试发布程序使用的对象文件，避免独立编译的 UI 测试掩盖增量构建中的类布局混用。
add_test(NAME sleekpr_designer_launch_test COMMAND sleekpr --smoke-test-designer)
set_tests_properties(sleekpr_designer_launch_test PROPERTIES TIMEOUT 20)
configure_qt_test_runtime(sleekpr_designer_launch_test)

if(MSVC AND CMAKE_GENERATOR STREQUAL "Ninja")
    add_test(NAME sleekpr_msvc_header_dependencies
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DNINJA_EXECUTABLE=${CMAKE_MAKE_PROGRAM}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_msvc_header_dependencies.cmake"
    )
endif()
