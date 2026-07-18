#include "assets.h"
#include "logging.h"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(gameresources);

namespace assets {
Image loadImage(const char *filename)
{
    std::string filenameString = filename;
    auto fs = cmrc::gameresources::get_filesystem();

    cmrc::file imageFile{};

    if (fs.is_file(filenameString)) {
        imageFile = fs.open(filenameString);
    } else [[unlikely]] {
        LOGWARN(Assets,
                "Failed to load asset {}, falling back to missing texture",
                filenameString);
        uassert(fs.is_file("missing.png"), "build is messed up");
        imageFile = fs.open("missing.png");
    }

    return LoadImageFromMemory(".png",
                               reinterpret_cast<const u8 *>(imageFile.begin()),
                               imageFile.size());
}

Texture uploadTexture(const char *filename)
{
    Image image = loadImage(filename);
    Texture out = LoadTextureFromImage(image);
    UnloadImage(image);
    return out;
}
} // namespace assets
