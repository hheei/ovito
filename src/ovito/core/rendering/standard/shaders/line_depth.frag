// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450
// Depth-only fragment shader for lines. Writes no color output, so it is compatible
// with a depth-only render target (no color attachments). Used by the excluded-depth
// pre-pass to capture the depth of ExcludeFromOutline line geometry (e.g. the
// simulation cell wireframe). Depth comes from rasterisation; nothing is written here.

void main()
{
}
