// Реализация stb_image (библиотека из одного заголовка) собирается здесь, один
// раз. Файлы движок читает сам (пути в UTF-8 на Windows), stb получает байты.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#define STBI_ONLY_HDR
#include <stb_image.h>
