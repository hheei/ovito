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

// Storage buffers for sorted particle rendering.
// Each buffer stores per-particle data; the sorted index buffer maps instance indices to particle indices.
// Used in sorted (painter's algorithm) vertex shaders instead of per-instance VBOs.

layout(std430, binding = 2) readonly buffer BufPositions      { float ssboPositionData[];        }; // vec3 per particle (3 floats)
layout(std430, binding = 3) readonly buffer BufRadii          { float ssboRadiusData[];          }; // float per particle
layout(std430, binding = 4) readonly buffer BufColors         { float ssboColorData[];           }; // vec3 per particle (3 floats)
layout(std430, binding = 5) readonly buffer BufTransparencies { float ssboTransparencyData[];    }; // float per particle
layout(std430, binding = 6) readonly buffer BufSelections     { uint  ssboSelectionData[];       }; // bit-packed: bit k = particle k%32 in word k/32
layout(std430, binding = 7) readonly buffer BufSortedIndices  { uint  ssboSortedIndexData[];     }; // uint per instance

// Helper accessors that resolve a sorted instance index to the actual particle's attribute.
vec3  ssboPosition(uint particleIdx)     { return vec3(ssboPositionData[3u * particleIdx], ssboPositionData[3u * particleIdx + 1u], ssboPositionData[3u * particleIdx + 2u]); }
float ssboRadius(uint particleIdx)       { return ssboRadiusData[particleIdx]; }
vec3  ssboColor(uint particleIdx)        { return vec3(ssboColorData[3u * particleIdx], ssboColorData[3u * particleIdx + 1u], ssboColorData[3u * particleIdx + 2u]); }
float ssboTransparency(uint particleIdx) { return ssboTransparencyData[particleIdx]; }
float ssboSelection(uint particleIdx)    { return float((ssboSelectionData[particleIdx >> 5u] >> (particleIdx & 31u)) & 1u); }
