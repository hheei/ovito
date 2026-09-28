// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Geometry snippet: 4-vertex screen-aligned quad for flat-shaded (billboard) cylinder rendering.
// The quad is oriented perpendicular to the view direction and spans the cylinder's length and width.
// gl_VertexIndex in [0..3] (TriangleStrip).
// Output varyings match the inputs of surface/cylinder_flat.glsl.

const vec2 QUAD_CORNERS[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

layout(location = 0) out vec4 color_fs;
layout(location = 1) flat out uint pickingId_fs;

void emitVertex(CylinderAttribs a, int vertexIndex)
{
    float radius = 0.5 * a.width;

    // Build a view-aligned coordinate frame in object space using the camera direction.
    // viewDirEyePos.xyz contains: viewing direction (orthographic) or camera position (perspective) in object space.
    // viewDirEyePos.w: 1.0 = perspective, 0.0 = orthographic.
    vec3 viewDir = (viewDirEyePos.w != 0.0) ? (viewDirEyePos.xyz - a.base) : viewDirEyePos.xyz;

    mat3 uv;
    uv[0] = 0.5 * (a.head - a.base);  // Half-axis along the cylinder
    uv[1] = normalize(cross(viewDir, uv[0])) * radius;
    uv[2] = vec3(0.0);

    // Interpolate color along the quad (corners 0,2 = base, corners 1,3 = head).
    vec4 mixedColor;
    if(vertexIndex == 0 || vertexIndex == 2)
        mixedColor = a.color1;
    else
        mixedColor = a.color2;

    // Project corner.
    vec3 center = a.base + uv[0];  // Cylinder midpoint
    vec3 pos = a.base + uv[0] + uv[0] * QUAD_CORNERS[vertexIndex].x + uv[1] * QUAD_CORNERS[vertexIndex].y;
    gl_Position = clipProjectionMatrix * modelViewMatrix * vec4(pos, 1.0);

    color_fs    = mixedColor;
    pickingId_fs = a.pickingId;
}
