// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Attribs snippet: SSBO-based per-instance attributes for sorted transparent cylinder rendering.
// All data comes from storage buffers indexed through ssboSortedIndices.
// Colors are always RGB (pseudo-colors are pre-converted on the CPU before upload).

layout(std430, binding = 3) readonly buffer BufPositions      { float ssboPositionData[]; };
layout(std430, binding = 4) readonly buffer BufWidths         { float ssboWidthData[];  };
layout(std430, binding = 5) readonly buffer BufColor1         { float ssboColor1Data[]; };
layout(std430, binding = 6) readonly buffer BufColor2         { float ssboColor2Data[]; };
layout(std430, binding = 7) readonly buffer BufTransp1        { float ssboTransp1Data[];};
layout(std430, binding = 8) readonly buffer BufTransp2        { float ssboTransp2Data[];};
layout(std430, binding = 9) readonly buffer BufSelections     { uint  ssboSelData[];    };
layout(std430, binding = 10) readonly buffer BufSortedIndices { uint  ssboSortedIndices[];};

uint getCylinderIndex() { return ssboSortedIndices[uint(gl_InstanceIndex)]; }

CylinderAttribs fetchCylinder(uint idx)
{
    vec3  base  = vec3(ssboPositionData[6u*idx], ssboPositionData[6u*idx+1u], ssboPositionData[6u*idx+2u]);
    vec3  head  = vec3(ssboPositionData[6u*idx+3u], ssboPositionData[6u*idx+4u], ssboPositionData[6u*idx+5u]);
    float width = ssboWidthData[idx];
    vec3  col1  = vec3(ssboColor1Data[3u*idx], ssboColor1Data[3u*idx+1u], ssboColor1Data[3u*idx+2u]);
    vec3  col2  = vec3(ssboColor2Data[3u*idx], ssboColor2Data[3u*idx+1u], ssboColor2Data[3u*idx+2u]);
    float t1    = ssboTransp1Data[idx];
    float t2    = ssboTransp2Data[idx];
    float sel   = float((ssboSelData[idx >> 5u] >> (idx & 31u)) & 1u);

    float alpha1 = clamp(1.0 - t1, 0.0, 1.0);
    float alpha2 = clamp(1.0 - t2, 0.0, 1.0);

    CylinderAttribs a;
    a.base      = base;
    a.head      = head;
    a.width     = width;
    a.pickingId = 0u;
    a.color1    = (sel != 0.0) ? selectionColor : vec4(col1, alpha1);
    a.color2    = (sel != 0.0) ? selectionColor : vec4(col2, alpha2);
    return a;
}
