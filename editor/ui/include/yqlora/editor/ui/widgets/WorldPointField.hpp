#pragma once

#include "yqlora/math/Math.hpp"

namespace yqlora::editor {

class EditorSession;

// Поля точек мира модального окна: число, кнопка "Из выбранного" и кнопка
// "Указать". "Указать" закрывает окно на время выбора точки щелчком в окне
// сцены (PointPick); когда выбор кончился, точка встаёт в своё поле, а
// окно просит открыть себя снова.
class WorldPointFields {
public:
    // До BeginPopupModal: true, пока идёт выбор (окно не рисуется). Ответ
    // ставится в поле, отмена оставляет поле как было.
    bool picking(EditorSession& session);

    // Окно надо открыть снова: выбор только что кончился. Один раз.
    bool takeReopen();

    // Поле точки внутри открытого окна.
    void field(const char* labelKey, Vec3& point, EditorSession& session);

private:
    // Поле, которое ждёт точку; nullptr - выбора нет.
    Vec3* m_target = nullptr;
    bool m_reopen = false;
};

} // namespace yqlora::editor
