/*
 * WinDrawLib
 * Copyright (c) 2015-2019 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "misc.h"
#include "backend-d2d.h"
#include "backend-dwrite.h"
#include "backend-gdix.h"
#include "lock.h"


void
wdDrawString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect,
             const WCHAR* pszText, int iTextLength, WD_HBRUSH hBrush,
             DWORD dwFlags)
{
    if(d2d_enabled()) {
        dwrite_font_t* font = (dwrite_font_t*) hFont;
        c_D2D1_POINT_2F origin = { pRect->x0, pRect->y0 };
        d2d_canvas_t* c = (d2d_canvas_t*) hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*) hBrush;
        c_IDWriteTextLayout* layout;
        c_D2D1_MATRIX_3X2_F old_matrix;

        layout = dwrite_create_text_layout(font->tf, pRect, pszText, iTextLength, dwFlags);
        if(layout == NULL) {
            WD_TRACE("wdDrawString: dwrite_create_text_layout() failed.");
            return;
        }

        if(c->flags & D2D_CANVASFLAG_RTL) {
            d2d_disable_rtl_transform(c, &old_matrix);
            origin.x = (float)c->width - pRect->x1;

            c_IDWriteTextLayout_SetReadingDirection(layout,
                    c_DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        }

        c_ID2D1RenderTarget_DrawTextLayout(c->target, origin, layout, b,
                (dwFlags & WD_STR_NOCLIP) ? 0 : c_D2D1_DRAW_TEXT_OPTIONS_CLIP);

        c_IDWriteTextLayout_Release(layout);

        if(c->flags & D2D_CANVASFLAG_RTL) {
            c_ID2D1RenderTarget_SetTransform(c->target, &old_matrix);
        }
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*) hCanvas;
        c_GpRectF r;
        c_GpFont* f = (c_GpFont*) hFont;
        c_GpBrush* b = (c_GpBrush*) hBrush;

        if(c->rtl) {
            gdix_rtl_transform(c);
            r.x = (float)(c->width-1) - pRect->x1;
        } else {
            r.x = pRect->x0;
        }
        r.y = pRect->y0;
        r.w = pRect->x1 - pRect->x0;
        r.h = pRect->y1 - pRect->y0;

        gdix_canvas_apply_string_flags(c, dwFlags);
        gdix_vtable->fn_DrawString(c->graphics, pszText, iTextLength,
                f, &r, c->string_format, b);

        if(c->rtl)
            gdix_rtl_transform(c);
    }
}

void
wdMeasureString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect,
                const WCHAR* pszText, int iTextLength, WD_RECT* pResult,
                DWORD dwFlags)
{
    if(d2d_enabled()) {
        dwrite_font_t* font = (dwrite_font_t*) hFont;
        c_IDWriteTextLayout* layout;
        c_DWRITE_TEXT_METRICS tm;

        layout = dwrite_create_text_layout(font->tf, pRect, pszText, iTextLength, dwFlags);
        if(layout == NULL) {
            WD_TRACE("wdMeasureString: dwrite_create_text_layout() failed.");
            return;
        }

        c_IDWriteTextLayout_GetMetrics(layout, &tm);

        pResult->x0 = pRect->x0 + tm.left;
        pResult->y0 = pRect->y0 + tm.top;
        pResult->x1 = pResult->x0 + tm.width;
        pResult->y1 = pResult->y0 + tm.height;

        c_IDWriteTextLayout_Release(layout);
    } else {
        HDC screen_dc;
        gdix_canvas_t* c;
        c_GpRectF r;
        c_GpFont* f = (c_GpFont*) hFont;
        c_GpRectF br;

        if(hCanvas != NULL) {
            c = (gdix_canvas_t*) hCanvas;
        } else {
            screen_dc = GetDCEx(NULL, NULL, DCX_CACHE);
            c = gdix_canvas_alloc(screen_dc, NULL, pRect->x1 - pRect->x0, FALSE);
            if(c == NULL) {
                WD_TRACE("wdMeasureString: gdix_canvas_alloc() failed.");
                pResult->x0 = 0.0f;
                pResult->y0 = 0.0f;
                pResult->x1 = 0.0f;
                pResult->y1 = 0.0f;
                return;
            }
        }

        if(c->rtl) {
            gdix_rtl_transform(c);
            r.x = (float)(c->width-1) - pRect->x1;
        } else {
            r.x = pRect->x0;
        }
        r.y = pRect->y0;
        r.w = pRect->x1 - pRect->x0;
        r.h = pRect->y1 - pRect->y0;

        gdix_canvas_apply_string_flags(c, dwFlags);
        gdix_vtable->fn_MeasureString(c->graphics, pszText, iTextLength, f, &r,
                                c->string_format, &br, NULL, NULL);

        if(c->rtl)
            gdix_rtl_transform(c);

        if(hCanvas == NULL) {
            gdix_canvas_free(c);
            ReleaseDC(NULL, screen_dc);
        }

        pResult->x0 = br.x;
        pResult->y0 = br.y;
        pResult->x1 = br.x + br.w;
        pResult->y1 = br.y + br.h;
    }
}

float
wdStringWidth(WD_HCANVAS hCanvas, WD_HFONT hFont, const WCHAR* pszText)
{
    const WD_RECT rcClip = { 0.0f, 0.0f, 10000.0f, 10000.0f };
    WD_RECT rcResult;

    wdMeasureString(hCanvas, hFont, &rcClip, pszText, wcslen(pszText),
                &rcResult, WD_STR_LEFTALIGN | WD_STR_NOWRAP);
    return WD_ABS(rcResult.x1 - rcResult.x0);
}

float
wdStringHeight(WD_HFONT hFont, const WCHAR* pszText)
{
    WD_FONTMETRICS metrics;

    wdFontMetrics(hFont, &metrics);
    return metrics.fLeading;
}
