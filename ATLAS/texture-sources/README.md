# Temporary terrain texture sources

The three PNGs (sand, ground and stone) were generated for this project using
Arena's image generation tool, not copied from a third-party texture library.
No third-party texture licence is being asserted; use is subject to the
applicable Arena service terms. Original dimensions: sand 1408×768, ground
1024×1024, stone 1408×768. They are temporary test assets.

`../tools/prepare_terrain_textures.py` centre-crops, downsamples to 512×512,
and applies cosine overlap across both axes to make each image repeatable.
It writes uncompressed, headerless RGBA8 assets for the Android app. This
crossfade reduces seams but may introduce visible repeated or blurred patterns;
visual testing on the target device is still needed.
