# Модуль игры проекта: собирает каждый .cpp в папке проекта, кроме скрытых
# папок (.build/ и других). Лежит в SDK, редактор передаёт папку проекта в
# YQLORA_PROJECT_DIR, поэтому у проекта нет своих файлов сборки.
cmake_minimum_required(VERSION 3.26)
project(game LANGUAGES CXX)

if(NOT YQLORA_PROJECT_DIR)
    message(FATAL_ERROR "Set YQLORA_PROJECT_DIR to the project folder")
endif()

find_package(yqlora REQUIRED)

# Путь Windows с "\" попал бы в созданный CMake скрипт проверки glob как экранирование.
file(TO_CMAKE_PATH "${YQLORA_PROJECT_DIR}" project_dir)

# CONFIGURE_DEPENDS: новый или удалённый файл Ninja замечает при сборке.
file(GLOB_RECURSE found CONFIGURE_DEPENDS
    RELATIVE ${project_dir}
    ${project_dir}/*.cpp
)
set(sources)
foreach(file IN LISTS found)
    # Путь от папки проекта: точка в начале любой его части - скрытая папка.
    if(NOT file MATCHES "(^|/)\\.")
        list(APPEND sources ${project_dir}/${file})
    endif()
endforeach()

yqlora_add_game_module(game ${sources})
# Заголовки подключаются и рядом с файлом, и от корня проекта.
target_include_directories(game PRIVATE ${project_dir})
# __FILE__ - путь от папки проекта: по нему скрипт стоит на полке своей папки
# (YQLORA_REGISTER_SCRIPT), а путь с машины разработчика не попадает в игру.
# TODO: у MSVC такого флага нет, там __FILE__ остаётся полным путём (редактор
# понимает и его, но путь попадёт в игру). Решить, когда появится сборка MSVC.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(game PRIVATE "-fmacro-prefix-map=${project_dir}/=")
endif()
