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

// Attribs snippet: SSBO-based attributes for sorted superquadric particles.
// Bindings 2-6: positions, radii, colors, transparencies, selections (same as spherical).
// Binding 7: sortedIndices.
// Binding 8: asphericalShapes (3 floats per particle).
// Binding 9: orientations (4 floats per particle).
// Binding 10: roundness (2 floats per particle).
// Provides: fetchParticle(uint idx) and getParticleIndex().

layout(std430, binding = 2)  readonly buffer BufPositions      { float ssboPositionData[];     };
layout(std430, binding = 3)  readonly buffer BufRadii          { float ssboRadiusData[];       };
layout(std430, binding = 4)  readonly buffer BufColors         { float ssboColorData[];        };
layout(std430, binding = 5)  readonly buffer BufTransparencies { float ssboTransparencyData[]; };
layout(std430, binding = 6)  readonly buffer BufSelections     { uint  ssboSelectionData[];    };
layout(std430, binding = 7)  readonly buffer BufSortedIndices  { uint  ssboSortedIndexData[];  };
layout(std430, binding = 8)  readonly buffer BufAspherical     { float ssboAsphericalData[];   };
layout(std430, binding = 9)  readonly buffer BufOrientations   { float ssboOrientationData[];  };
layout(std430, binding = 10) readonly buffer BufRoundness      { float ssboRoundnessData[];    };

uint getParticleIndex() { return ssboSortedIndexData[uint(gl_InstanceIndex)]; }

ParticleAttribs fetchParticle(uint idx)
{
    vec3  pos    = vec3(ssboPositionData[3u*idx], ssboPositionData[3u*idx+1u], ssboPositionData[3u*idx+2u]);
    float r      = ssboRadiusData[idx];
    vec3  col    = vec3(ssboColorData[3u*idx], ssboColorData[3u*idx+1u], ssboColorData[3u*idx+2u]);
    float transp = ssboTransparencyData[idx];
    float sel    = float((ssboSelectionData[idx >> 5u] >> (idx & 31u)) & 1u);
    float alpha  = clamp(1.0 - transp, 0.0, 1.0);
    vec3  axes   = vec3(ssboAsphericalData[3u*idx], ssboAsphericalData[3u*idx+1u], ssboAsphericalData[3u*idx+2u]);
    vec4  quat   = vec4(ssboOrientationData[4u*idx], ssboOrientationData[4u*idx+1u],
                        ssboOrientationData[4u*idx+2u], ssboOrientationData[4u*idx+3u]);
    vec2  rnd    = vec2(ssboRoundnessData[2u*idx], ssboRoundnessData[2u*idx+1u]);

    ParticleAttribs a;
    a.position      = pos;
    a.radius        = r;
    a.color         = (sel != 0.0) ? selectionColor : vec4(col, alpha);
    a.orientation   = quat;
    a.principalAxes = axes;
    a.roundness     = rnd;
    a.pickingId     = 0u;
    return a;
}
