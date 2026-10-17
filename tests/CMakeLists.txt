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
    ../resources/sleekpr-ui.qrc
    ../src/app/AppTheme.cpp
    ../src/app/TableAdvancedEditorPanel.cpp
    ../include/sleekpr/app/TableAdvancedEditorPanel.h
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
list(TRANSFORM SLEEKPR_LABEL_APP_SOURCES PREPEND "../")
add_executable(sleekpr_label_app_tests
    tst_label_designer.cpp
    ${SLEEKPR_LABEL_APP_SOURCES}
)
set_target_properties(sleekpr_label_app_tests PROPERTIES AUTORCC ON)
target_link_libraries(sleekpr_label_app_tests PRIVATE
    sleekpr_core Qt6::Widgets Qt6::PrintSupport Qt6::Test
)
target_compile_definitions(sleekpr_label_app_tests PRIVATE SLEEKPR_ENABLE_TABLE_DESIGNER=0)
add_test(NAME sleekpr_label_app_tests COMMAND sleekpr_label_app_tests)
configure_qt_test_runtime(sleekpr_label_app_tests)

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
