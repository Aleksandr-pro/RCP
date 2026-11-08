#include "MaterialConfig.h"
#include "../physics/MaterialNames.h"

#include <android/log.h>
#include <locale>
#include <sstream>
#include <string>

namespace atlas
{
    bool loadMaterialConfig(AAssetManager* manager, const char* path,
                            physics::ContactMaterial& contact, float& friction)
    {
        constexpr const char* tag = "ATLAS";
        constexpr off_t maxBytes = 256;
        if (!manager || !path)
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: отсутствует AssetManager или путь.");
            return false;
        }
        AAsset* asset = AAssetManager_open(manager, path, AASSET_MODE_STREAMING);
        if (!asset)
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: файл не найден: %s", path);
            return false;
        }
        const off_t length = AAsset_getLength(asset);
        if (length <= 0 || length > maxBytes)
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: неверный размер файла %s: %lld (1–256 байт).", path, static_cast<long long>(length));
            AAsset_close(asset);
            return false;
        }
        std::string contents(static_cast<size_t>(length), '\0');
        size_t offset = 0;
        while (offset < contents.size())
        {
            const int read = AAsset_read(asset, &contents[offset], contents.size() - offset);
            if (read <= 0 || static_cast<size_t>(read) > contents.size() - offset)
            {
                __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: не удалось полностью прочитать %s.", path);
                AAsset_close(asset);
                return false;
            }
            offset += static_cast<size_t>(read);
        }
        AAsset_close(asset);
        std::istringstream input(contents);
        input.imbue(std::locale::classic());
        std::string bodyName, surfaceName, extra;
        if (!(input >> bodyName >> surfaceName) || (input >> extra))
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: в %s ожидалось ровно два имени.", path);
            return false;
        }
        physics::BodyMaterial body{};
        physics::SurfaceMaterial surface{};
        if (!physics::parseBodyMaterial(bodyName, body))
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: неизвестный материал тела '%s' в %s.", bodyName.c_str(), path);
            return false;
        }
        if (!physics::parseSurfaceMaterial(surfaceName, surface))
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: неизвестная поверхность '%s' в %s.", surfaceName.c_str(), path);
            return false;
        }
        physics::ContactMaterial parsed{};
        if (!physics::getContactMaterial(body, surface, parsed))
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: неподдерживаемая пара '%s' и '%s' в %s.", bodyName.c_str(), surfaceName.c_str(), path);
            return false;
        }
        float parsedFriction = 0.0f;
        if (!physics::getSlidingFriction(body, surface, parsedFriction))
        {
            __android_log_print(ANDROID_LOG_ERROR, tag, "Материалы: трение недоступно для '%s' и '%s'.", bodyName.c_str(), surfaceName.c_str());
            return false;
        }
        contact = parsed;
        friction = parsedFriction;
        __android_log_print(ANDROID_LOG_INFO, tag, "Материалы загружены: %s %s, restitution=%.2f, restSpeed=%.2f.", bodyName.c_str(), surfaceName.c_str(), contact.restitution, contact.restSpeed);
        return true;
    }
}
