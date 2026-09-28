# SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
# SPDX-License-Identifier: GPL-3.0-only OR MIT

# This CMake script compiles the user manual for OVITO
# by transforming the reStructured text files into HTML using Sphinx.

# Controls the generation of the user manual.
OPTION(OVITO_BUILD_DOCUMENTATION "Build the user manual together with OVITO" "OFF")
OPTION(OVITO_BUILD_DOCUMENTATION_STRICT "Treat warnings as errors when building the documentation" "ON")
OPTION(OVITO_PREVIEW_LLM_DOCUMENTATION "Show LLM/AI-specific content in the human-readable documentation (HTML)" "OFF")

IF(OVITO_BUILD_APP)
    # Create destination directories.
    FILE(MAKE_DIRECTORY "${OVITO_SHARE_DIRECTORY}/docs/manual")
    FILE(MAKE_DIRECTORY "${OVITO_SHARE_DIRECTORY}/docs/manual/html")

    # Locate a Python interpreter.
    # Note: Sphinx 4.x requires at least Python 3.6.
    OVITO_FIND_PACKAGE(Python3 REQUIRED COMPONENTS Interpreter)

    # Tell Intersphinx where to find the scripting reference manual files when building the user manual.
    # Use the existing Sphinx output tree of the scripting reference manual for resolving cross-references if it exists
    # in the python/ sub-directory (only the case when building OVITO Pro).
    # Otherwise, fall back to using the online version of the scripting reference at https://ovito.org/docs/current/python/.
    # For further information, see the 'intersphinx_mapping' configuration variable in the file 'conf.py'.
    IF(OVITO_BUILD_PROFESSIONAL)
        SET(OVITO_PYDOC_INTERSPHINX_LOCATION "${OVITO_SHARE_DIRECTORY}/docs/manual/html/python/objects.inv")
    ENDIF()

    # Output settings for the HTML variant of the manual (the default, shipped form).
    SET(_OVITO_MANUAL_HTML_BUILDER "html")
    SET(_OVITO_MANUAL_HTML_OUTPUT_DIR "${OVITO_SHARE_DIRECTORY}/docs/manual/html/")
    IF(OVITO_BUILD_DOCUMENTATION_STRICT) # Turn warnings into errors by default
        SET(_OVITO_MANUAL_HTML_STRICT_MODE "-W")
    ELSE()
        SET(_OVITO_MANUAL_HTML_STRICT_MODE "")
    ENDIF()
    IF(OVITO_PREVIEW_LLM_DOCUMENTATION)
        # If the developers set the OVITO_PREVIEW_LLM_DOCUMENTATION option, then the HTML user manual will include
        # content that is intended for LLM/AI readers. This is meant to be used for internal preview purposes only.
        SET(_OVITO_MANUAL_EXTRA_TAGS "-t" "ovito_llm")
    ENDIF()

    # Output settings for the Markdown variant of the manual, generated only occasionally on
    # demand using the 'sphinx-markdown-builder' package (must be installed into the Sphinx
    # Python environment). This shared format is also honored by the scripting reference target
    # (see cmake/Documentation.cmake).
    #
    # Note: The output must go to a build-only directory, NOT below ${OVITO_SHARE_DIRECTORY}. The Markdown
    # documentation is an internal build product that is never installed. On macOS the share directory is
    # located inside the application bundle, which INSTALL(TARGETS Ovito) copies wholesale, so anything
    # placed there would end up in the shipped package regardless of the absence of an INSTALL() rule.
    SET(_OVITO_MANUAL_MARKDOWN_BUILDER "markdown")
    SET(_OVITO_MANUAL_MARKDOWN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/doc_markdown/")

    # Build the user manual (HTML) using Sphinx.
    ADD_CUSTOM_TARGET(documentation
        # Create the destination directory for the generated files:
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_OVITO_MANUAL_HTML_OUTPUT_DIR}"
        # Run Sphinx command to generate the output files:
        COMMAND "${CMAKE_COMMAND}" -E env "OVITO_PYDOC_INTERSPHINX_LOCATION=${OVITO_PYDOC_INTERSPHINX_LOCATION}"
            "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sphinx-build.py"  # Run the sphinx-build.py script through the system's Python interpreter
            "."                                           # Sphinx source directory
            "${_OVITO_MANUAL_HTML_OUTPUT_DIR}"            # Destination directory
            -b "${_OVITO_MANUAL_HTML_BUILDER}"
            -t ovito_human                                # Tag used by '.. only::' directives to select content intended for human readers
            ${_OVITO_MANUAL_EXTRA_TAGS}                   # Additional tags, e.g., for LLM/AI content
            -a -E                                         # Always write all output files. Don’t use a saved environment.
            -n                                            # Run in nit-picky mode
            "${_OVITO_MANUAL_HTML_STRICT_MODE}"           # Turn warnings into errors
            # Additional config settings passed to Sphinx, which are added to the options found in conf.py:
            -D "version=${OVITO_VERSION_STRING}"
            -D "release=${OVITO_VERSION_STRING}"
            -D "copyright=${OVITO_CURRENT_YEAR} OVITO GmbH, Germany"
        WORKING_DIRECTORY "${Ovito_SOURCE_DIR}/docs/manual/"
        COMMENT "Generating user manual")

    # Extract the link targets from the Intersphinx inventory file and write them to a text file.
    # The file will be used by the OVITO application to open the correct HTML page corresponding to
    # a help topic ID. See UserInterface::openHelpTopic() method for further information.
    ADD_CUSTOM_COMMAND(TARGET documentation POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E env "${Python3_EXECUTABLE}"
            -m sphinx.ext.intersphinx "${OVITO_SHARE_DIRECTORY}/docs/manual/html/objects.inv"
            > "${OVITO_SHARE_DIRECTORY}/docs/manual/html/objects.txt"
        WORKING_DIRECTORY "${Ovito_SOURCE_DIR}/docs/manual/"
        COMMENT "Extracting help topic index from Intersphinx inventory")

    # Build the user manual (Markdown) using Sphinx. Built only on demand, not part of the default build.
    ADD_CUSTOM_TARGET(documentation_markdown
        # Create the destination directory for the generated files:
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_OVITO_MANUAL_MARKDOWN_OUTPUT_DIR}"
        # Run Sphinx command to generate the output files:
        COMMAND "${CMAKE_COMMAND}" -E env "OVITO_PYDOC_INTERSPHINX_LOCATION=${OVITO_PYDOC_INTERSPHINX_LOCATION}"
            "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sphinx-build.py"  # Run the sphinx-build.py script through the system's Python interpreter
            "."                                           # Sphinx source directory
            "${_OVITO_MANUAL_MARKDOWN_OUTPUT_DIR}"        # Destination directory
            -b "${_OVITO_MANUAL_MARKDOWN_BUILDER}"
            -t ovito_llm                                  # Tag used by '.. only::' directives to select content intended for LLM/AI readers
            -a -E                                         # Always write all output files. Don’t use a saved environment.
            -n                                            # Run in nit-picky mode
            # Note: Not using strict mode for Markdown output, because the 'sphinx-markdown-builder'
            # package does not support all reStructuredText features.
            # Additional config settings passed to Sphinx, which are added to the options found in conf.py:
            -D "version=${OVITO_VERSION_STRING}"
            -D "release=${OVITO_VERSION_STRING}"
            -D "copyright=${OVITO_CURRENT_YEAR} OVITO GmbH, Germany"
        WORKING_DIRECTORY "${Ovito_SOURCE_DIR}/docs/manual/"
        COMMENT "Generating user manual (Markdown)")

    # Are we building OVITO Pro?
    IF(TARGET scripting_documentation_prerun)
        # Build user manual after the scripting reference manual was built for the first time (pre-run),
        # because Intersphinx need its inventory database.
        ADD_DEPENDENCIES(documentation scripting_documentation_prerun)
        # Build the final version of the scripting reference after the user manual, to make
        # Intersphinx cross-references from the scripting reference to the user manual work.
        ADD_DEPENDENCIES(scripting_documentation documentation)
    ENDIF()
    IF(TARGET scripting_documentation_markdown_prerun)
        ADD_DEPENDENCIES(documentation_markdown scripting_documentation_markdown_prerun)
        ADD_DEPENDENCIES(scripting_documentation_markdown documentation_markdown)
    ENDIF()

    # In deferred documentation mode the documentation cannot be generated as part of the normal build,
    # because the scripting reference it is cross-linked with requires an interpreter that is only available
    # after the installation step. It must be built explicitly via the 'documentation_all' target instead.
    IF(OVITO_BUILD_DOCUMENTATION AND NOT OVITO_DOCS_OVITOS_EXECUTABLE)
        # Run the documentation target automatically as part of the build process.
        ADD_DEPENDENCIES(Ovito documentation)
    ENDIF()

    # Install the generated HTML documentation files alongside with the application.
    # Exclude the PowerPoint slides from the installation, because they are not needed by the end user.
    INSTALL(DIRECTORY "${OVITO_SHARE_DIRECTORY}/docs/manual/html/" DESTINATION "${OVITO_RELATIVE_SHARE_DIRECTORY}/docs/manual/html/" PATTERN "*.pptx" EXCLUDE)
ENDIF()
