// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// FragOut snippet: depth-only pass — no color output.
// Used for the outline depth pre-pass; depth is written via gl_FragDepth in main().
// Declaring no color outputs avoids D3D12 warning #679 when the render target
// has no color attachment.

void writeOut(SurfaceSample)
{
    // No color attachments in this render pass; nothing to write.
}
