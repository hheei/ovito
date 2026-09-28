# -*- coding: utf-8 -*-
# SPDX-FileCopyrightText: 2021 Alexander Stukowski
# SPDX-License-Identifier: GPL-3.0-only OR MIT

import sys

if __name__ == '__main__':

    try:
        from sphinx.cmd.build import main
    except ImportError: # Fallback for older Sphinx versions:
        from sphinx import main

    sys.exit(main())