#ifndef __GAME_ASSETS_H__
#define __GAME_ASSETS_H__

#include <raylib.h>

namespace assets {
/// Loads raw pixel data from a file into an allocated buffer.
/// Remember to call UnloadImage() on the result.
/// File path will be relative to the `assets/` folder, e.g.
/// loadImage("missing.png")
///
/// To upload this image to the GPU as a texture, call LoadTextureFromImage()
Image loadImage(const char *filename);
/// Textures are on the GPU. this will allocate a buffer, decode the image into
/// raw pixel date, upload that to the GPU, then free the buffer.
/// Remember to call UnloadTexture() on the result.
/// File path will be relative to the `assets/` folder, e.g.
/// loadImage("missing.png")
Texture uploadTexture(const char *filename);
}; // namespace assets

#endif
