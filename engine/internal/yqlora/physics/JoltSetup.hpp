#pragma once

#include "yqlora/core/Export.hpp"

namespace yqlora {

// Глобальная настройка Jolt, одна на процесс: распределитель памяти,
// Factory, регистрация типов, его трассировка и проверки - в наш лог,
// проверка, что движок и библиотека Jolt собраны одинаково. Делает её
// первый держатель, снимает последний: Jolt настроен, пока жив хоть один.
class YQLORA_API JoltSetup {
public:
    JoltSetup();
    ~JoltSetup();

    JoltSetup(const JoltSetup&) = delete;
    JoltSetup& operator=(const JoltSetup&) = delete;

    // Сколько держателей сейчас в процессе.
    static int holders();
};

} // namespace yqlora
