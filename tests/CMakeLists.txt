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
        # CTest 独立启动测试进程时不会自动继承 Qt bin 路径，所有 Qt 测试都需要补齐 DLL 搜索路径。
        set_tests_properties(${target_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endfunction()
else()
    function(configure_qt_test_runtime target_name)
        # 非 Windows 平台不需要额外设置 PATH，保留同名函数让每个测试目标的配置方式一致。
    endfunction()
endif()

configure_qt_test_runtime(sleekpr_core_tests)

add_executable(sleekpr_app_tests
    tst_app.cpp
    ../src/app/AppTheme.cpp
    ../src/app/TableColumnEditorPanel.cpp
    ../include/sleekpr/app/TableColumnEditorPanel.h
    ../src/app/TableColumnTextCodec.cpp
    ../include/sleekpr/app/TableColumnTextCodec.h
    ../src/app/TemplateDesignerCommand.cpp
    ../src/app/FieldPresetManagerWindow.cpp
    ../src/app/PaperSpecManagerWindow.cpp
    ../src/app/SettingsWindow.cpp
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
configure_qt_test_runtime(sleekpr_app_tests)

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
