/*
    Copyright 2022-2024 Hydr8gon
    Copyright 2026 Project 3DS contributors
    Modified 2026 by Project 3DS contributors.

    This file is part of Project 3DS, a derivative work of Project DS
    <https://github.com/Hydr8gon/Project-DS>.

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#ifndef P3DS_RENDER_CONTEXT_H
#define P3DS_RENDER_CONTEXT_H

#include <cstdint>
#include <string>

#include <citro2d.h>

namespace p3ds {

/** The artwork the game draws. */
enum Texture
{
    TextureTriangle = 0,
    TextureCircle,
    TextureCross,
    TextureSquare,
    TextureTriangleHole,
    TextureCircleHole,
    TextureCrossHole,
    TextureSquareHole,
    TextureSliderLeft,
    TextureSliderRight,
    TextureSliderLeftHeld,
    TextureSliderRightHeld,
    TextureSliderLeftHole,
    TextureSliderRightHole,
    TextureSliderLeftHeldHole,
    TextureSliderRightHeldHole,
    TextureArrow,
    TextureHold,
    TextureCool,
    TextureFine,
    TextureSafe,
    TextureSad,
    TextureMiss,
    TextureLifeFull,
    TextureLifeEmpty,
    TextureMultiTriangle,
    TextureMultiCircle,
    TextureMultiCross,
    TextureMultiSquare,
    TextureMultiTriangleHole,
    TextureMultiCircleHole,
    TextureMultiCrossHole,
    TextureMultiSquareHole,
    TextureMultiSliderLeft,
    TextureMultiSliderRight,
    TextureMultiSliderLeftHeld,
    TextureMultiSliderRightHeld,
    TextureMultiSliderLeftHole,
    TextureMultiSliderRightHole,
    TextureMultiSliderLeftHeldHole,
    TextureMultiSliderRightHeldHole,
    TextureCount
};

/**
 * The drawing of the game.
 *
 * The two screens are separate render targets, and each one is drawn once
 * per frame.
 */
class RenderContext
{
public:
    RenderContext();

    // The context owns the GPU textures, the font and the text buffer, so a
    // copy would free the same handles twice
    RenderContext(const RenderContext &) = delete;
    RenderContext &operator=(const RenderContext &) = delete;

    /** Sets the drawing up. */
    bool init();

    /** Cleans the drawing up. */
    void exit();

    /** Starts a frame. */
    void beginFrame();

    /** Ends a frame. */
    void endFrame();

    /** Starts drawing on the top screen. */
    void beginTop();

    /** Stops drawing on the top screen. */
    void endTop();

    /** Starts drawing on the bottom screen. */
    void beginBottom();

    /** Stops drawing on the bottom screen. */
    void endBottom();

    /** Fills the top screen with a colour. */
    void clearTop(uint32_t colour);

    /** Fills the bottom screen with a colour. */
    void clearBottom(uint32_t colour);

    /** Draws a piece of artwork. */
    void drawTexture(Texture texture, float x, float y, float angle = 0.0f, float depth = 0.5f);

    /** Draws a piece of artwork in the middle of it. */
    void drawTextureCentered(Texture texture, float x, float y, float angle = 0.0f, float depth = 0.5f);

    /** Draws a piece of artwork at a size of its own. */
    void drawTextureScaled(Texture texture, float x, float y, float width, float height, float depth = 0.5f);

    /** Draws a line of text. */
    void drawText(float x, float y, const std::string &text, uint32_t colour = 0xFFFFFFFF,
                  float scale = 0.4f, float depth = 0.9f);

    /** Draws a line of text in the middle of it. */
    void drawTextCentered(float x, float y, const std::string &text, uint32_t colour = 0xFFFFFFFF,
                          float scale = 0.4f, float depth = 0.9f);

    /** The width of a line of text. */
    float textWidth(const std::string &text, float scale = 0.4f);

    /** The scale that makes the font a readable size. */
    float fontScale() const { return scale; }

private:
    void loadTextures();

    C3D_RenderTarget *topTarget = nullptr;
    C3D_RenderTarget *bottomTarget = nullptr;
    C2D_Font font = nullptr;
    C2D_TextBuf textBuffer = nullptr;
    C2D_Image images[TextureCount];
    C3D_Tex textures[TextureCount];
    Tex3DS_Texture imported[TextureCount];
    float scale = 0.4f;
};

/**
 * The colour the background of the gameplay screen is made of.
 * citro2d packs colours as 0xAABBGGRR
 */
constexpr uint32_t BackgroundColour = 0xFF848484;

/** The colour the background of the interface screen is made of. */
constexpr uint32_t InterfaceBackgroundColour = 0xFF000000;

} // namespace p3ds

#endif // P3DS_RENDER_CONTEXT_H
