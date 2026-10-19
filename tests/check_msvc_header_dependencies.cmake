if(NOT DEFINED BUILD_DIR OR NOT DEFINED NINJA_EXECUTABLE)
    message(FATAL_ERROR "缺少构建目录或 Ninja 路径")
endif()

function(require_recorded_header object_file header_name)
    execute_process(
        COMMAND "${NINJA_EXECUTABLE}" -C "${BUILD_DIR}" -t deps "${object_file}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE dependencies
        ERROR_VARIABLE diagnostics
        ENCODING UTF-8
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "无法读取 ${object_file} 的依赖：${diagnostics}")
    endif()
    string(REPLACE "\\" "/" normalized "${dependencies}")
    string(FIND "${normalized}" "${header_name}" header_offset)
    if(header_offset EQUAL -1)
        # 类布局变化后若调用方未重编译，new 分配的旧大小会直接损坏堆，不能只验证单独重编译的测试目标。
        message(FATAL_ERROR "${object_file} 未记录关键头文件 ${header_name}，增量构建可能混用不同类布局。\n${dependencies}")
    endif()
endfunction()

require_recorded_header("CMakeFiles/sleekpr.dir/src/app/TemplateWorkspacePanel.cpp.obj" "TemplatePreviewLabel.h")
require_recorded_header("CMakeFiles/sleekpr.dir/src/app/main.cpp.obj" "TemplateDesignerWindow.h")
require_recorded_header("CMakeFiles/sleekpr.dir/src/app/main.cpp.obj" "PrintWorkbenchWindow.h")
require_recorded_header("CMakeFiles/sleekpr.dir/src/app/TemplateDesignerWindow.cpp.obj" "TemplateCanvasEditor.h")
