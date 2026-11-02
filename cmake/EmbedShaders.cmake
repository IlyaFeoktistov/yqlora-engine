# Вшивает файлы шейдеров в заголовок C++: таблица kEmbeddedShaders из пар
# (путь относительно ROOT, текст). Запускается при сборке:
#   cmake -DOUTPUT=<header> -DROOT=<папка> -DSHADERS=<файлы через |> -P EmbedShaders.cmake
# Так исходники шейдеров лежат отдельными файлами, а собранной программе они не нужны на диске.

string(REPLACE "|" ";" SHADERS "${SHADERS}")
set(DELIMITER "yqlora_shader")
set(CONTENT "// Сгенерировано cmake/EmbedShaders.cmake из файлов шейдеров, не править руками.
#pragma once

#include <array>
#include <string_view>
#include <utility>

namespace yqlora::shaders {

inline constexpr std::array kEmbeddedShaders{
")

foreach(SHADER ${SHADERS})
    file(RELATIVE_PATH PATH ${ROOT} ${SHADER})
    file(READ ${SHADER} SOURCE)
    string(FIND "${SOURCE}" ")${DELIMITER}\"" CLASH)
    if(NOT CLASH EQUAL -1)
        message(FATAL_ERROR "Shader ${SHADER} contains the raw string delimiter")
    endif()
    string(APPEND CONTENT "    std::pair<std::string_view, std::string_view>{\"${PATH}\", R\"${DELIMITER}(${SOURCE})${DELIMITER}\"},\n")
endforeach()

string(APPEND CONTENT "};\n\n} // namespace yqlora::shaders\n")

# Пишем, только если поменялось: иначе пересобирались бы все, кто его подключает.
if(EXISTS ${OUTPUT})
    file(READ ${OUTPUT} OLD)
endif()
if(NOT "${OLD}" STREQUAL "${CONTENT}")
    file(WRITE ${OUTPUT} "${CONTENT}")
endif()
