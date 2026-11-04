#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/Record.hpp"

namespace yqlora {

// Атмосфера планеты: из неё физическое небо, свет солнца у земли и дымка
// вдали (см. docs/architecture/materials.md, "Atmosphere"). Значения по
// умолчанию - Земля. Рассеяние и поглощение - на мегаметр (1000 км), чтобы
// числа были удобными; высоты и радиусы - в км.
struct Atmosphere {
    bool enabled = true;
    // Доля света, которую отражает земля (sRGB): подсвечивает небо снизу.
    Vec3 groundAlbedo{0.3f};
    float planetRadius = 6360.0f;
    float atmosphereHeight = 100.0f;
    // Рэлей: рассеяние молекулами воздуха, синее сильнее красного.
    Vec3 rayleighScattering{5.802f, 13.558f, 33.1f};
    // Высота, на которой плотность падает в e раз.
    float rayleighHeight = 8.0f;
    // Ми: аэрозоли и пыль, почти одинаково для всех цветов.
    float mieScattering = 3.996f;
    float mieAbsorption = 4.4f;
    float mieHeight = 1.2f;
    // Насколько свет Ми идёт вперёд: 0 - во все стороны, ближе к 1 - ореол у солнца.
    float mieAnisotropy = 0.8f;
    // Озон в слое на высотах 10-40 км: поглощает красный и делает небо глубже.
    Vec3 ozoneAbsorption{0.65f, 1.881f, 0.085f};
    // Дымка вдали (воздушная перспектива): 0 - нет, 1 - физически, больше -
    // гуще, для стиля.
    float aerialPerspective = 1.0f;
};

} // namespace yqlora

YQLORA_RECORD(
    yqlora::Atmosphere,
    field("enabled", &yqlora::Atmosphere::enabled),
    field("groundAlbedo", &yqlora::Atmosphere::groundAlbedo, PropertyHint::Color),
    field("planetRadius", &yqlora::Atmosphere::planetRadius).range(1.0f, 100000.0f),
    field("atmosphereHeight", &yqlora::Atmosphere::atmosphereHeight).range(1.0f, 1000.0f),
    field("rayleighScattering", &yqlora::Atmosphere::rayleighScattering).range(0.0f, 1000.0f),
    field("rayleighHeight", &yqlora::Atmosphere::rayleighHeight).range(0.1f, 100.0f),
    field("mieScattering", &yqlora::Atmosphere::mieScattering).range(0.0f, 1000.0f),
    field("mieAbsorption", &yqlora::Atmosphere::mieAbsorption).range(0.0f, 1000.0f),
    field("mieHeight", &yqlora::Atmosphere::mieHeight).range(0.1f, 100.0f),
    field("mieAnisotropy", &yqlora::Atmosphere::mieAnisotropy).range(0.0f, 0.99f),
    field("ozoneAbsorption", &yqlora::Atmosphere::ozoneAbsorption).range(0.0f, 100.0f),
    field("aerialPerspective", &yqlora::Atmosphere::aerialPerspective).range(0.0f, 10.0f)
)
