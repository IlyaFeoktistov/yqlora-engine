include(FetchContent)

# Некоторые зависимости объявляют старый cmake_minimum_required, CMake 4.x без этого падает.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# SDL3 собираем статически: на Windows не нужно таскать SDL3.dll рядом с exe.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE
)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.2
    GIT_SHALLOW TRUE
)

# Всё качаем через git, а не URL: curl внутри некоторых сборок CMake (например, WinLibs)
# не видит системные сертификаты Windows, а git использует их всегда.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)

# Dear ImGui (ветка docking) для интерфейса редактора. Своего CMakeLists у неё нет,
# поэтому цели собираем сами: ядро плюс платформенная часть на SDL3.
# Часть для конкретного графического API собирает соответствующий бэкенд.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b-docking
    GIT_SHALLOW TRUE
)

# ImGuizmo: манипуляторы перемещения, поворота и масштаба в окне сцены редактора.
# Её CMakeLists собирает лишнее, поэтому только скачиваем (SOURCE_SUBDIR без
# CMakeLists) и собираем один нужный файл сами.
FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG 1.10
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-build
)

# cgltf: чтение glTF 2.0 (.gltf, .glb) для мешей из файлов. Один заголовок без
# CMakeLists: только скачиваем, реализацию собирает движок в одном своём файле.
FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.15
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-build
)

# stb_image: картинки PNG, JPG, TGA, BMP для текстур. Тегов у stb нет,
# поэтому закреплён коммит. Реализацию собирает движок, как у cgltf.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
    SOURCE_SUBDIR do-not-build
)

# MikkTSpace: касательные для карт нормалей, как их ждёт glTF и программы,
# в которых пекут карты. Тегов нет, закреплён коммит; один файл на C
# собирается отдельной библиотекой ниже.
FetchContent_Declare(mikktspace
    GIT_REPOSITORY https://github.com/mmikk/MikkTSpace.git
    GIT_TAG 3e895b49d05ea07e4c2133156cfa94369e19e409
    SOURCE_SUBDIR do-not-build
)

# glslang: шейдеры GLSL 450 в SPIR-V. Только компилятор: без оптимизатора
# (ему нужен SPIRV-Tools), HLSL, программ и тестов. SYSTEM: наши строгие
# предупреждения не распространяются на чужие заголовки.
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
# Со сведениями о типах и исключениями: наш код наследует классы glslang
# (их typeinfo требует проверка vptr у UBSan в пресете asan), а исключение из
# чтения файла шейдера проходит через код glslang.
set(ENABLE_RTTI ON CACHE BOOL "" FORCE)
set(ENABLE_EXCEPTIONS ON CACHE BOOL "" FORCE)
FetchContent_Declare(glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG vulkan-sdk-1.4.341.0
    GIT_SHALLOW TRUE
    SYSTEM
)

# SPIRV-Cross: отражение SPIR-V (движок) и перевод в GLSL 330 (бэкенд OpenGL).
# Перевод в HLSL добавится с бэкендом D3D12.
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG vulkan-sdk-1.4.341.0
    GIT_SHALLOW TRUE
    SYSTEM
)

# Jolt Physics: физика (docs/architecture/physics.md). Только внутри движка:
# в открытых заголовках его типов нет, поэтому его определения (точность,
# наборы команд процессора) не обязаны совпадать с кодом игры.
# Наши флаги компиляции он не переписывает, предупреждения - не наши.
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# Исключения процессора включаются только в его тестах; в игре ими ведает она сама.
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
# Как у glslang: наши классы наследуют интерфейсы Jolt, а проверке vptr у
# UBSan (пресет asan) нужны их typeinfo.
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
# Проверки Jolt - в отладочной сборке, как наши assert.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(USE_ASSERTS ON CACHE BOOL "" FORCE)
else()
    set(USE_ASSERTS OFF CACHE BOOL "" FORCE)
endif()
# Игра должна идти на любом x86-64 с SSE4.2 (процессоры с 2008 года), а не
# только там, где есть AVX2.
set(USE_AVX OFF CACHE BOOL "" FORCE)
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT OFF CACHE BOOL "" FORCE)
set(USE_TZCNT OFF CACHE BOOL "" FORCE)
set(USE_F16C OFF CACHE BOOL "" FORCE)
set(USE_FMADD OFF CACHE BOOL "" FORCE)
# Отрисовка отладки и профилировщик Jolt не нужны: коллайдеры рисует
# редактор, сериализация объектов Jolt - тоже. Вычисления на GPU - позже.
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
    SYSTEM
)

FetchContent_MakeAvailable(SDL3 glm nlohmann_json imgui imguizmo cgltf stb mikktspace glslang spirv_cross JoltPhysics)
# Входит в разделяемую библиотеку движка.
set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)

# SYSTEM: наши строгие предупреждения не распространяются на чужой код.
add_library(cgltf_headers INTERFACE)
target_include_directories(cgltf_headers SYSTEM INTERFACE ${cgltf_SOURCE_DIR})
add_library(stb_headers INTERFACE)
target_include_directories(stb_headers SYSTEM INTERFACE ${stb_SOURCE_DIR})
# Код входит в разделяемую библиотеку движка, поэтому позиционно-независимый.
add_library(mikktspace STATIC ${mikktspace_SOURCE_DIR}/mikktspace.c)
target_include_directories(mikktspace SYSTEM PUBLIC ${mikktspace_SOURCE_DIR})
set_target_properties(mikktspace PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ImGui и ImGuizmo входят в библиотеку движка, чтобы у всех модулей был один
# контекст ImGui. Внутри неё функции ImGui экспортируются, снаружи импортируются.
if(WIN32)
    set(YQLORA_DLL_EXPORT "__declspec(dllexport)")
    set(YQLORA_DLL_IMPORT "__declspec(dllimport)")
else()
    set(YQLORA_DLL_EXPORT "__attribute__((visibility(\"default\")))")
    set(YQLORA_DLL_IMPORT "__attribute__((visibility(\"default\")))")
endif()

# Только заголовки. SYSTEM: наши строгие предупреждения не распространяются на чужой код.
add_library(imgui_headers INTERFACE)
target_include_directories(imgui_headers SYSTEM INTERFACE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp
    ${imguizmo_SOURCE_DIR}/src
)

# OBJECT, а не STATIC: из статического архива в библиотеку попало бы только то,
# что вызывает сам движок, а редактору нужны и ImGuizmo, и обёртки над std::string.
add_library(imgui OBJECT
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)
target_link_libraries(imgui PUBLIC imgui_headers PRIVATE SDL3::SDL3-static)
target_compile_definitions(imgui PRIVATE "IMGUI_API=${YQLORA_DLL_EXPORT}")

if(YQLORA_BUILD_TESTS)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.12
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(doctest)
endif()
