#pragma once

#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora {

// Шейдеры проекта (.yshader, обе стадии в одном файле) в ShaderData.
// "#include "file"" ищется рядом с файлом внутри проекта,
// "#include <yqlora/...>" - в библиотеке движка. Ошибки компиляции, профиля
// и пометок - ошибка ресурса со всеми сообщениями "файл:строка: текст".
// Версия профиля, под которую написан шейдер, хранится в .meta
// (profileVersion); шейдер другой версии не загружается, его переводит
// перевод проекта.
class YQLORA_API ShaderLoader final : public ResourceLoader {
public:
    std::span<const std::string_view> extensions() const override;
    std::string_view kind() const override;
    LoadResult load(const LoadRequest& request) const override;
};

} // namespace yqlora
