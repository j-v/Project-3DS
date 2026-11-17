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

#include "render_context.h"

#include <3ds.h>

#include <cstring>

#include "arrow_t3x.h"
#include "ui_bcfnt.h"
#include "circle_t3x.h"
#include "circle_hole_t3x.h"
#include "cool_t3x.h"
#include "cross_t3x.h"
#include "cross_hole_t3x.h"
#include "fine_t3x.h"
#include "hold_t3x.h"
#include "life_empty_t3x.h"
#include "life_full_t3x.h"
#include "miss_t3x.h"
#include "multi_circle_t3x.h"
#include "multi_circle_hole_t3x.h"
#include "multi_cross_t3x.h"
#include "multi_cross_hole_t3x.h"
#include "multi_slider_l_t3x.h"
#include "multi_slider_l_hole_t3x.h"
#include "multi_slider_lh_t3x.h"
#include "multi_slider_lh_hole_t3x.h"
#include "multi_slider_r_t3x.h"
#include "multi_slider_r_hole_t3x.h"
#include "multi_slider_rh_t3x.h"
#include "multi_slider_rh_hole_t3x.h"
#include "multi_square_t3x.h"
#include "multi_square_hole_t3x.h"
#include "multi_triangle_t3x.h"
#include "multi_triangle_hole_t3x.h"
#include "sad_t3x.h"
#include "safe_t3x.h"
#include "slider_l_t3x.h"
#include "slider_l_hole_t3x.h"
#include "slider_lh_t3x.h"
#include "slider_lh_hole_t3x.h"
#include "slider_r_t3x.h"
#include "slider_r_hole_t3x.h"
#include "slider_rh_t3x.h"
#include "slider_rh_hole_t3x.h"
#include "square_t3x.h"
#include "square_hole_t3x.h"
#include "triangle_t3x.h"
#include "triangle_hole_t3x.h"

namespace p3ds {

RenderContext::RenderContext()
{
    for (int i = 0; i < TextureCount; i++)
    {
        // Both halves of the image have to be set: citro2d passes the
        // texture pointer to the GPU when it draws the image, and a garbage
        // pointer there faults inside citro3d
        images[i].tex = nullptr;
        images[i].subtex = nullptr;
        imported[i] = nullptr;
    }
}

bool RenderContext::init()
{
    // citro2d is built on top of citro3d, and it does not set citro3d up by
    // itself: without it, the texture environments of the GPU are never
    // allocated, and the first draw faults
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))
        return false;

    if (!C2D_Init(4096))
        return false;

    topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    if (!topTarget || !bottomTarget)
        return false;

    // The font is baked from the project's TTF into a BCFNT, whose glyph
    // sheets live in memory that the texture unit can address. The font of
    // the console cannot be used instead: its glyph sheets live in the shared
    // memory, which the texture unit cannot reach.
    font = C2D_FontLoadFromMem(ui_bcfnt, ui_bcfnt_size);

    if (!font)
        return false;

    textBuffer = C2D_TextBufNew(4096);

    if (!textBuffer)
        return false;

    // The font is built at 16 points and the interface draws it at a smaller
    // size, which gives text of about eight pixels.
    scale = 0.72f;

#ifdef P3DS_DIAG
    {
        FILE *log = fopen("sdmc:/project-3ds-diag.log", "a");

        if (log)
        {
            fprintf(log, "font: scale %f\n", (double)scale);
            fclose(log);
        }
    }
#endif

    loadTextures();

    // citro2d does not set the GPU up for 2D drawing by itself: the shader
    // program, the vertex buffer and the texture environments have to be
    // bound before anything is drawn
    C2D_Prepare();

    // A sprite's quad is drawn even where its artwork is transparent, and the
    // depth buffer records the whole quad. Discarding fully transparent pixels
    // keeps a sprite from hiding the artwork that is drawn behind it.
    C3D_AlphaTest(true, GPU_GREATER, 0);

    return true;
}

void RenderContext::loadTextures()
{
    static const struct
    {
        Texture texture;
        const void *data;
        unsigned int size;
    } textures[] =
    {
        { TextureTriangle, triangle_t3x, triangle_t3x_size },
        { TextureCircle, circle_t3x, circle_t3x_size },
        { TextureCross, cross_t3x, cross_t3x_size },
        { TextureSquare, square_t3x, square_t3x_size },
        { TextureTriangleHole, triangle_hole_t3x, triangle_hole_t3x_size },
        { TextureCircleHole, circle_hole_t3x, circle_hole_t3x_size },
        { TextureCrossHole, cross_hole_t3x, cross_hole_t3x_size },
        { TextureSquareHole, square_hole_t3x, square_hole_t3x_size },
        { TextureSliderLeft, slider_l_t3x, slider_l_t3x_size },
        { TextureSliderRight, slider_r_t3x, slider_r_t3x_size },
        { TextureSliderLeftHeld, slider_lh_t3x, slider_lh_t3x_size },
        { TextureSliderRightHeld, slider_rh_t3x, slider_rh_t3x_size },
        { TextureSliderLeftHole, slider_l_hole_t3x, slider_l_hole_t3x_size },
        { TextureSliderRightHole, slider_r_hole_t3x, slider_r_hole_t3x_size },
        { TextureSliderLeftHeldHole, slider_lh_hole_t3x, slider_lh_hole_t3x_size },
        { TextureSliderRightHeldHole, slider_rh_hole_t3x, slider_rh_hole_t3x_size },
        { TextureArrow, arrow_t3x, arrow_t3x_size },
        { TextureHold, hold_t3x, hold_t3x_size },
        { TextureCool, cool_t3x, cool_t3x_size },
        { TextureFine, fine_t3x, fine_t3x_size },
        { TextureSafe, safe_t3x, safe_t3x_size },
        { TextureSad, sad_t3x, sad_t3x_size },
        { TextureMiss, miss_t3x, miss_t3x_size },
        { TextureLifeFull, life_full_t3x, life_full_t3x_size },
        { TextureLifeEmpty, life_empty_t3x, life_empty_t3x_size },
        { TextureMultiTriangle, multi_triangle_t3x, multi_triangle_t3x_size },
        { TextureMultiCircle, multi_circle_t3x, multi_circle_t3x_size },
        { TextureMultiCross, multi_cross_t3x, multi_cross_t3x_size },
        { TextureMultiSquare, multi_square_t3x, multi_square_t3x_size },
        { TextureMultiTriangleHole, multi_triangle_hole_t3x, multi_triangle_hole_t3x_size },
        { TextureMultiCircleHole, multi_circle_hole_t3x, multi_circle_hole_t3x_size },
        { TextureMultiCrossHole, multi_cross_hole_t3x, multi_cross_hole_t3x_size },
        { TextureMultiSquareHole, multi_square_hole_t3x, multi_square_hole_t3x_size },
        { TextureMultiSliderLeft, multi_slider_l_t3x, multi_slider_l_t3x_size },
        { TextureMultiSliderRight, multi_slider_r_t3x, multi_slider_r_t3x_size },
        { TextureMultiSliderLeftHeld, multi_slider_lh_t3x, multi_slider_lh_t3x_size },
        { TextureMultiSliderRightHeld, multi_slider_rh_t3x, multi_slider_rh_t3x_size },
        { TextureMultiSliderLeftHole, multi_slider_l_hole_t3x, multi_slider_l_hole_t3x_size },
        { TextureMultiSliderRightHole, multi_slider_r_hole_t3x, multi_slider_r_hole_t3x_size },
        { TextureMultiSliderLeftHeldHole, multi_slider_lh_hole_t3x, multi_slider_lh_hole_t3x_size },
        { TextureMultiSliderRightHeldHole, multi_slider_rh_hole_t3x, multi_slider_rh_hole_t3x_size }
    };

    for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); i++)
    {
        int index = textures[i].texture;

        // The textures go into the VRAM of the console
        imported[index] = Tex3DS_TextureImport(textures[i].data, textures[i].size, &this->textures[index],
                                               nullptr, true);

        // A texture that could not be loaded is simply left out
        if (imported[index])
        {
            images[index].tex = &this->textures[index];
            images[index].subtex = Tex3DS_GetSubTexture(imported[index], 0);
        }
    }
}

void RenderContext::exit()
{
    for (int i = 0; i < TextureCount; i++)
    {
        if (imported[i])
            Tex3DS_TextureFree(imported[i]);
    }

    if (textBuffer)
        C2D_TextBufDelete(textBuffer);

    if (font)
        C2D_FontFree(font);

    C2D_Fini();
    C3D_Fini();
}

void RenderContext::beginTop()
{
    C2D_SceneBegin(topTarget);
}

void RenderContext::endTop()
{
    C2D_Flush();
}

void RenderContext::beginBottom()
{
    C2D_SceneBegin(bottomTarget);
}

void RenderContext::endBottom()
{
    C2D_Flush();
}

void RenderContext::beginFrame()
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void RenderContext::endFrame()
{
    C3D_FrameEnd(0);
}

void RenderContext::clearTop(uint32_t colour)
{
    C2D_TargetClear(topTarget, colour);
}

void RenderContext::clearBottom(uint32_t colour)
{
    C2D_TargetClear(bottomTarget, colour);
}

void RenderContext::drawTexture(Texture texture, float x, float y, float angle, float depth)
{
    C2D_Image image = images[texture];

    if (!image.tex || !image.subtex)
        return;

    if (angle == 0.0f)
    {
        C2D_DrawImageAt(image, x, y, depth, nullptr, 1.0f, 1.0f);
        return;
    }

    // C2D_DrawImageAtRotated places the centre of the texture,
    // so the position is moved to the centre when there is a rotation
    C2D_DrawImageAtRotated(image, x + image.subtex->width / 2.0f,
                           y + image.subtex->height / 2.0f, depth, angle, nullptr, 1.0f, 1.0f);
}

void RenderContext::drawTextureCentered(Texture texture, float x, float y, float angle, float depth)
{
    C2D_Image image = images[texture];

    if (!image.tex || !image.subtex)
        return;

    C2D_DrawImageAtRotated(image, x, y, depth, angle, nullptr, 1.0f, 1.0f);
}

void RenderContext::drawTextureScaled(Texture texture, float x, float y, float width, float height, float depth)
{
    C2D_Image image = images[texture];

    if (!image.tex || !image.subtex)
        return;

    C2D_DrawParams params;
    memset(&params, 0, sizeof(params));
    params.pos.x = x;
    params.pos.y = y;
    params.pos.w = width;
    params.pos.h = height;
    params.depth = depth;

    C2D_DrawImage(image, &params, nullptr);
}

void RenderContext::drawText(float x, float y, const std::string &text, uint32_t colour,
                             float scale, float depth)
{
    C2D_Text c2dText;
    memset(&c2dText, 0, sizeof(c2dText));
    C2D_TextBufClear(textBuffer);
    C2D_TextFontParse(&c2dText, font, textBuffer, text.c_str());

    C2D_DrawText(&c2dText, C2D_AlignLeft | C2D_WithColor, x, y, depth, scale, scale, colour);
}

void RenderContext::drawTextCentered(float x, float y, const std::string &text, uint32_t colour,
                                     float scale, float depth)
{
    // citro2d centres a line by reading the word list of the text, which
    // only exists when the text is word wrapped, so the line is centred here
    // instead
    float width = textWidth(text, scale);

    drawText(x - width / 2.0f, y, text, colour, scale, depth);
}

float RenderContext::textWidth(const std::string &text, float scale)
{
    C2D_Text c2dText;
    memset(&c2dText, 0, sizeof(c2dText));
    C2D_TextBufClear(textBuffer);
    C2D_TextFontParse(&c2dText, font, textBuffer, text.c_str());

    float width = 0, height = 0;
    C2D_TextGetDimensions(&c2dText, scale, scale, &width, &height);
    return width;
}

} // namespace p3ds
