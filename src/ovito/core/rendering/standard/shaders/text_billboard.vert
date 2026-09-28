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

#version 450
#extension GL_GOOGLE_include_directive : enable
#include "scene_params.glsl"
#include "text_billboard_draw_params.glsl"

// Per-instance attributes.
layout(location = 0) in vec3 instancePosition; ///< Label anchor point (model space).
layout(location = 1) in float instanceRadius;  ///< World-space radius of the annotated element.
layout(location = 2) in vec4 instanceUVRect;   ///< Atlas region (u0,v0,u1,v1), v grows downward.
layout(location = 3) in vec2 instanceSizePx;   ///< On-screen extent of the label in device pixels.

// Output to fragment shader.
layout(location = 0) out vec2 v_texcoord;

void main()
{
    // Quad corner from the vertex index (triangle strip):
    // c = (0,0) top-left, (1,0) top-right, (0,1) bottom-left, (1,1) bottom-right,
    // where c.y = 1 is DOWN in window coordinates.
    vec2 c = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));

    // Project the anchor point. All work below happens in uncorrected (y-up NDC) clip space;
    // the backend-specific correction (Vulkan/D3D y-flip) is applied at the very end, so no
    // isYUpInNDC branch is needed.
    vec4 viewPos = modelViewMatrix * vec4(instancePosition, 1.0);
    vec4 clip = projectionMatrix * viewPos;

    // Give the label the depth of the anchor shifted toward the camera by the element's radius,
    // so that it surfaces in front of the glyph it annotates instead of being buried inside.
    // The shift is applied to the view-space z coordinate only - which is what the depth buffer
    // stores - because a shift along the view ray would shorten z by merely radius*cos(angle)
    // and leave labels of off-axis elements partially clipped by their own glyphs. The screen
    // position and the on-screen size are still derived from the unshifted anchor, so the label
    // does not move or scale. The user-adjustable depth offset lifts the label by a further absolute
    // world-space margin to avoid depth-test ties with the frontmost fragment of the glyph itself.
    // Being absolute, it also lifts the labels of elements that report no radius at all, e.g. voxel
    // grid cells and surface mesh vertices, which a margin relative to the radius could not.
    float zShifted = viewPos.z + instanceRadius + depthOffset;
    if(isPerspective != 0)
        zShifted = min(zShifted, -1e-4);    // Keep the depth reference in front of the eye.
    vec4 clipShifted = projectionMatrix * vec4(viewPos.xy, zShifted, 1.0);
    clip.z = clipShifted.z / clipShifted.w * clip.w;

    // The on-screen size of the element's radius at the label's depth, in device pixels.
    // Works for both projection types: for orthographic projections clip.w is 1 and
    // projectionMatrix[1][1] = 1/fieldOfView.
    float radiusPx = instanceRadius * projectionMatrix[1][1] / clip.w * (viewportHeight * 0.5);

    // Offset from the anchor to this quad corner in window pixels (y down): the quad corner
    // relative to the aligned quad origin, plus the radius shift away from the element, plus
    // the constant user offset.
    vec2 px = (c - alignmentFactor) * instanceSizePx + shiftDirection * radiusPx + pixelOffset;

    // Convert the pixel offset to clip space (window y-down to NDC y-up) and scale by w so the
    // label keeps a constant apparent size on screen.
    clip.xy += vec2(px.x * 2.0 / viewportWidth, -px.y * 2.0 / viewportHeight) * clip.w;

    gl_Position = clipSpaceCorrMatrix * clip;

    // The atlas image's v axis grows downward, exactly like the window y axis, so the corner
    // coordinate maps to the uv rect directly without a flip.
    v_texcoord = mix(instanceUVRect.xy, instanceUVRect.zw, c);
}
