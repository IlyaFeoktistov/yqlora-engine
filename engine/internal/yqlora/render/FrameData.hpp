#pragma once

#include "yqlora/material/MaterialData.hpp"
#include "yqlora/material/MaterialOverrides.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/render/Atmosphere.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/Light3D.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/Layers.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace yqlora {

struct MeshData;
struct TextureData;
class ViewPass;

struct CameraData {
    Mat4 view{1.0f};
    Mat4 projection{1.0f};
    Vec3 position{0.0f};
};

// Экспозиция и тональная компрессия вида. Вид без камеры (тесты, свои
// кадры) нейтрален: яркость как есть, без сжатия.
struct ViewExposure {
    // Множитель яркости в нитах (exposureFactor камеры).
    float factor = 1.0f;
    ToneMapping toneMapping = ToneMapping::None;
};

// Один вид кадра: камера, чем стереть перед ней и какие слои она рисует.
struct ViewData {
    CameraData camera;
    // Перед рисованием вид заливается цветом background. У видов камер
    // сцены здесь стоит очистка камеры, а новая камера по умолчанию рисует
    // за объектами небо (CameraClear::Sky).
    CameraClear clear = CameraClear::ColorAndDepth;
    // В линейном RGB, как и всё, что рисуется в HDR-цель.
    Vec4 background{0.0f, 0.0f, 0.0f, 1.0f};
    LayerMask mask = kAllLayers;
    ViewExposure exposure{};
    // Проходы владельца вида поверх сцены, по порядку. Живут дольше кадра.
    std::vector<ViewPass*> passes{};
};

// Текстура материала в кадре: ресурс или, если его нет или он не
// загрузился, текстура по умолчанию поля.
struct FrameTexture {
    rhi::BindingSlot slot{};
    ResourceId id = 0;
    std::uint64_t revision = 0;
    // nullptr - рисуется fallback.
    const TextureData* data = nullptr;
    DefaultTexture fallback = DefaultTexture::White;
};

// Материал кадра вместе с его шейдером и текстурами, как их нашёл Extract:
// рендерер кэша ресурсов не читает. Материал со сломанным или пропавшим
// шейдером здесь уже заменён материалом Error.
struct FrameMaterial {
    ResourceId id = 0;
    std::uint64_t revision = 0;
    const MaterialData* data = nullptr;
    ResourceId shaderId = 0;
    std::uint64_t shaderRevision = 0;
    const ShaderData* shader = nullptr;
    // По текстурам шейдера, в его порядке.
    std::vector<FrameTexture> textures{};
};

// Замены материала у одного слота (MaterialOverrides) на этот кадр: копия
// значений и текстуры шейдера с заменами поверх текстур материала.
struct FrameOverrides {
    MaterialOverrides values{};
    std::vector<FrameTexture> textures{};
};

// Один подмеш в кадре. Данные меша живут в кэше ресурсов до конца кадра.
struct DrawItem {
    static constexpr std::size_t kNoOverrides = static_cast<std::size_t>(-1);

    const MeshData* mesh = nullptr;
    // По ID и ревизии рендерер узнаёт, что его копия меша в видеопамяти устарела.
    ResourceId meshId = 0;
    std::uint64_t meshRevision = 0;
    // Диапазон индексов подмеша.
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    Mat4 world{1.0f};
    // Границы меша в мире: по ним Prepare отсекает невидимое.
    Bounds bounds{};
    // Индекс в FrameData::materials.
    std::size_t material = 0;
    // Индекс в FrameData::overrides или kNoOverrides.
    std::size_t overrides = kNoOverrides;
    int layer = 0;
    NodeId node = 0; // чей это меш: по нему редактор узнаёт выбранный объект
};

// Источник света в мире, готовый для шейдера.
struct LightData {
    LightKind kind = LightKind::Directional;
    Vec3 position{0.0f};
    // Куда свет идёт, единичный: ось -Z узла.
    Vec3 direction{0.0f, 0.0f, -1.0f};
    // Линейный цвет, умноженный на интенсивность: люксы у направленного,
    // канделы у точечного и прожектора.
    Vec3 color{1.0f};
    // У точечного и прожектора.
    float range = 0.0f;
    // У прожектора: косинусы внутреннего и внешнего угла от оси.
    float cosInner = 1.0f;
    float cosOuter = 1.0f;
    // Солнце сцены: направленный, первый в FrameData::lights.
    bool sun = false;
};

// Окружение кадра (узел Environment): небо, свет неба и атмосфера. Без
// окружения - ни неба, ни света неба, ни атмосферы.
struct EnvironmentData {
    // Материал неба в FrameData::materials; nullopt - неба нет.
    std::optional<std::size_t> sky{};
    // Чем светит небо: материал неба, который рисуется в куб для освещения
    // (у источника Sky - сам sky, у Gradient и Color - встроенный градиент),
    // и его замены в FrameData::overrides. nullopt - света неба нет.
    std::optional<std::size_t> ambientSky{};
    std::size_t ambientOverrides = DrawItem::kNoOverrides;
    float ambientIntensity = 1.0f;
    Atmosphere atmosphere{.enabled = false};
};

// Сколько источников шейдер видит за кадр (массивы блока Frame).
inline constexpr std::size_t kMaxDirectionalLights = 4;
inline constexpr std::size_t kMaxPunctualLights = 32;

// Всё, что рендереру нужно знать о кадре. Данные собирает extractFrame,
// рендерер их рисует: виды по очереди, у каждого свои очистка и маска.
// Дерево узлов до бэкенда не доходит.
struct FrameData {
    std::vector<ViewData> views;
    std::vector<DrawItem> drawItems;
    // Материалы, которыми рисуются drawItems, каждый один раз.
    std::vector<FrameMaterial> materials;
    std::vector<FrameOverrides> overrides;
    // Не больше kMaxDirectionalLights направленных и kMaxPunctualLights
    // точечных и прожекторов вместе.
    std::vector<LightData> lights;
    EnvironmentData environment;
};

} // namespace yqlora
