// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#version 450

layout(location = 0) in vec4 color_fs;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = color_fs;
}
