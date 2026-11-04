#pragma once

// Иконки из шрифта Lucide (assets/fonts/lucide.ttf, пакет lucide-static 1.54.0).
// Шрифт подмешан к основному, поэтому иконку можно ставить прямо в текст:
// std::string(icons::kFolder) + " Assets". Коды взяты из font/info.json пакета,
// в комментарии - имя иконки на lucide.dev.
namespace yqlora::editor::icons {

// Инструменты окна сцены.
inline constexpr const char* kSelect = "\uE1C3";         // mouse-pointer-2
inline constexpr const char* kMove = "\uE2E5";           // move-3d
inline constexpr const char* kRotate = "\uE2EA";         // rotate-3d
inline constexpr const char* kScale = "\uE2EB";          // scale-3d
inline constexpr const char* kLocal = "\uE061";          // box
inline constexpr const char* kGlobal = "\uE0E8";         // globe
inline constexpr const char* kSnapGrid = "\uE2B5";       // magnet
inline constexpr const char* kAlignToSurface = "\uE44E"; // arrow-up-from-dot
inline constexpr const char* kDropDown = "\uE06D";       // chevron-down
inline constexpr const char* kPick = "\uE345";           // circle-dot: окно выбора ресурса
inline constexpr const char* kShadowCascades = "\uE529"; // layers-3: отладка каскадов теней

// Игра в редакторе.
inline constexpr const char* kPlay = "\uE13C";  // play
inline constexpr const char* kPause = "\uE12E"; // pause
inline constexpr const char* kStop = "\uE167";  // square

// Типы узлов в дереве сцены.
inline constexpr const char* kNode = "\uE345";               // circle-dot
inline constexpr const char* kNode3D = "\uE2FE";             // axis-3d
inline constexpr const char* kCamera3D = "\uE1A5";           // video
inline constexpr const char* kDirectionalLight3D = "\uE178"; // sun
inline constexpr const char* kPointLight3D = "\uE1C2";       // lightbulb
inline constexpr const char* kSpotLight3D = "\uE0D3";        // flashlight
inline constexpr const char* kEnvironment = "\uE216";        // cloud-sun

// Файлы и действия.
inline constexpr const char* kFolder = "\uE0D7";       // folder
inline constexpr const char* kFolderOpen = "\uE247";   // folder-open
inline constexpr const char* kImport = "\uE334";       // folder-input
inline constexpr const char* kFile = "\uE0C0";         // file
inline constexpr const char* kScene = "\uE29B";        // clapperboard
inline constexpr const char* kCodeFile = "\uE0C3";     // file-code
inline constexpr const char* kModelFile = "\uE310";    // file-box
inline constexpr const char* kImageFile = "\uE31C";    // file-image
inline constexpr const char* kShaderFile = "\uE59C";   // blend
inline constexpr const char* kSkyShader = "\uE179";    // sunrise
inline constexpr const char* kMaterialFile = "\uE1DD"; // palette
inline constexpr const char* kMesh = "\uE524";         // cuboid
inline constexpr const char* kBuild = "\uE0EC";        // hammer
inline constexpr const char* kMainScene = "\uE176";    // star
inline constexpr const char* kAdd = "\uE13D";          // plus
inline constexpr const char* kAddFolder = "\uE0D9";    // folder-plus
inline constexpr const char* kAddFile = "\uE0C9";      // file-plus
inline constexpr const char* kRefresh = "\uE145";      // refresh-cw
inline constexpr const char* kDelete = "\uE18E";       // trash-2
inline constexpr const char* kRename = "\uE1F9";       // pencil
inline constexpr const char* kSave = "\uE14D";         // save
inline constexpr const char* kSearch = "\uE151";       // search
inline constexpr const char* kClear = "\uE28F";        // eraser
inline constexpr const char* kCopy = "\uE09E";         // copy

// Лог: панель и уровни сообщений.
inline constexpr const char* kLog = "\uE45F";     // scroll-text
inline constexpr const char* kInfo = "\uE0F9";    // info
inline constexpr const char* kWarning = "\uE193"; // alert-triangle (triangle-alert)
inline constexpr const char* kError = "\uE084";   // circle-x

} // namespace yqlora::editor::icons
