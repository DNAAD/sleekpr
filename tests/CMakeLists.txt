add_executable(sleekpr_core_tests
    tst_core.cpp
)

target_link_libraries(sleekpr_core_tests
    PRIVATE
        sleekpr_core
        Qt6::Test
)

add_test(NAME sleekpr_core_tests COMMAND sleekpr_core_tests)

add_executable(sleekpr_app_tests
    tst_app.cpp
    ../src/app/SettingsWindow.cpp
    ../src/app/TemplateDragCoordinateMapper.cpp
    ../src/app/TemplateElementHitTester.cpp
    ../src/app/TemplatePreviewLabel.cpp
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

if(WIN32)
    # CTest 不会继承开发命令行里追加的 Qt bin 路径，窗口测试需要显式补齐运行时 DLL 搜索路径。
    set_tests_properties(sleekpr_app_tests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

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

if(WIN32)
    # HTTP 测试也链接 Qt 运行时，CTest 独立启动时需要同样补齐 DLL 搜索路径。
    set_tests_properties(sleekpr_http_tests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
