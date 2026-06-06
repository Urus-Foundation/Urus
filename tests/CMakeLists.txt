# CTest hookup — runs the PowerShell test runner.
if(WIN32)
    add_test(NAME urus_smoke
        COMMAND powershell -ExecutionPolicy Bypass -File
                "${CMAKE_SOURCE_DIR}/scripts/run-tests.ps1"
                -Binary "$<TARGET_FILE:urusc>")
else()
    add_test(NAME urus_smoke
        COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/run-tests.sh"
                "$<TARGET_FILE:urusc>")
endif()
