#pragma once

#include <filesystem>
#include <string>

namespace yqlora {

class TypeInfo;
class TypeRegistry;

} // namespace yqlora

namespace yqlora::editor {

// Тексты о типе для человека. Дерево сцены, инспектор и код показывают имя
// типа; имя на языке человека и описание - в окне выбора типа и в его
// причинах, рядом с именем типа.

// Имя типа на языке человека (type.<имя>.name); без перевода - имя типа.
std::string typeDisplayName(const TypeInfo& type);
// То же по имени типа; нет такого типа в реестре - само имя.
std::string typeDisplayName(const TypeRegistry& registry, const std::string& typeName);

// Описание типа: перевод type.<имя>.description, иначе заданное при
// регистрации, иначе у скрипта - комментарий над его классом (исходник
// ищется от projectDirectory). Пусто - нет.
std::string typeDescription(const TypeInfo& type, const std::filesystem::path& projectDirectory);

// Исходник скрипта на диске: TypeInfo::sourceFile от папки проекта (так его
// пишет сборка модуля игры) или уже полный путь. Пусто у типов движка.
std::filesystem::path scriptSourcePath(const TypeInfo& type, const std::filesystem::path& projectDirectory);

} // namespace yqlora::editor
