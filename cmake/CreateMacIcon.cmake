if(NOT DEFINED INPUT_PNG OR NOT EXISTS "${INPUT_PNG}")
    message(FATAL_ERROR "INPUT_PNG must point to the application icon PNG")
endif()
if(NOT DEFINED OUTPUT_ICNS)
    message(FATAL_ERROR "OUTPUT_ICNS must be set")
endif()

find_program(SIPS_EXECUTABLE sips)
find_program(ICONUTIL_EXECUTABLE iconutil)
if(NOT SIPS_EXECUTABLE OR NOT ICONUTIL_EXECUTABLE)
    message(FATAL_ERROR "sips and iconutil are required to build the macOS icon")
endif()

set(ICONSET_DIR "${OUTPUT_ICNS}.iconset")
file(REMOVE_RECURSE "${ICONSET_DIR}")
file(MAKE_DIRECTORY "${ICONSET_DIR}")

function(make_icon SIZE NAME)
    execute_process(
        COMMAND "${SIPS_EXECUTABLE}" -z "${SIZE}" "${SIZE}" "${INPUT_PNG}"
                --out "${ICONSET_DIR}/${NAME}"
        RESULT_VARIABLE SIPS_RESULT
        OUTPUT_QUIET
        ERROR_VARIABLE SIPS_ERROR
    )
    if(NOT SIPS_RESULT EQUAL 0)
        message(FATAL_ERROR "sips failed for ${NAME}: ${SIPS_ERROR}")
    endif()
endfunction()

make_icon(16   icon_16x16.png)
make_icon(32   icon_16x16@2x.png)
make_icon(32   icon_32x32.png)
make_icon(64   icon_32x32@2x.png)
make_icon(128  icon_128x128.png)
make_icon(256  icon_128x128@2x.png)
make_icon(256  icon_256x256.png)
make_icon(512  icon_256x256@2x.png)
make_icon(512  icon_512x512.png)
make_icon(1024 icon_512x512@2x.png)

execute_process(
    COMMAND "${ICONUTIL_EXECUTABLE}" -c icns "${ICONSET_DIR}"
            -o "${OUTPUT_ICNS}"
    RESULT_VARIABLE ICONUTIL_RESULT
    ERROR_VARIABLE ICONUTIL_ERROR
)
if(NOT ICONUTIL_RESULT EQUAL 0)
    message(FATAL_ERROR "iconutil failed: ${ICONUTIL_ERROR}")
endif()

file(REMOVE_RECURSE "${ICONSET_DIR}")
