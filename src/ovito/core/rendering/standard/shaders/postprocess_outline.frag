////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

// Post-process composite fragment shader.
//
// Inputs:
//   binding 1: colorTex       — RGBA8 intermediate scene color (from the main scene pass)
//   binding 2: highlightTex   — RGBA8 highlight silhouette (HighlightLayer rendered depth-test-disabled)
//   binding 3: outlineLayerTex — RGBA8 premultiplied outline layer (from OutlinesRhi compute pipeline)
//
// Effects applied in order:
//   1. Passthrough scene color.
//   2. If outlineEnabled: blend premultiplied outline layer (One, OneMinusSrcAlpha).
//   3. If hasHighlight: dilate highlightTex by silhouetteWidth pixels; draw highlight outline ring.

#version 450

layout(std140, binding = 0) uniform PostProcessParams {
    int   isYUpInNDC;
    int   hasHighlight;
    int   silhouetteWidth;
    int   outlineEnabled;
    vec4  outlineColor;     // kept for layout compatibility; color is baked into outlineLayerTex
    int   outlineWidth;
    float minDepthDiff;
    float maxDepthDiff;
    int   isPerspective;
    float nearPlane;
    float farPlane;
    int   _padDepthRange; // unused (was depthZeroToOne)
    int   isYUpInFramebuffer;
    int   outlineMinWidth;
    int   _pad0, _pad1, _pad2;
    vec4  highlightColor;   // color for HighlightLayer silhouette outline (always red)
};

layout(binding = 1) uniform sampler2D colorTex;        // Intermediate scene color (RGBA8).
layout(binding = 2) uniform sampler2D highlightTex;    // Highlight silhouette mask (RGBA8).
layout(binding = 3) uniform sampler2D outlineLayerTex; // Premultiplied RGBA8 from OutlinesRhi.

layout(location = 0) in vec2 uv_fs;
layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 result = texture(colorTex, uv_fs);

    // ── Outline layer composite ─────────────────────────────────────────────────
    // The outline layer is premultiplied alpha; blend with (One, OneMinusSrcAlpha).
    if(outlineEnabled != 0) {
        vec4 layer = texture(outlineLayerTex, uv_fs);
        result = layer + result * (1.0 - layer.a);
    }

    // ── Highlight silhouette outline ────────────────────────────────────────────
    if(hasHighlight != 0) {
        vec2 texelSize = 1.0 / vec2(textureSize(highlightTex, 0));
        float inner = textureLod(highlightTex, uv_fs, 0.0).a;

        // Dilate the highlight silhouette by silhouetteWidth pixels.
        float maxNeighbor = inner;  // seed with center, so (0,0) can be skipped in the loop
        int w = silhouetteWidth;
        for(int dx = -w; dx <= w; ++dx) {
            for(int dy = -w; dy <= w; ++dy) {
                if(dx == 0 && dy == 0) continue;
                maxNeighbor = max(maxNeighbor,
                    textureLod(highlightTex, uv_fs + vec2(float(dx), float(dy)) * texelSize, 0.0).a);
            }
        }

        // Outline ring = pixels in the dilation shell not already inside the silhouette.
        float outlineAlpha = maxNeighbor * (1.0 - inner);
        result = mix(result, vec4(highlightColor.rgb, 1.0), outlineAlpha * highlightColor.a);
    }

    fragColor = result;
}
