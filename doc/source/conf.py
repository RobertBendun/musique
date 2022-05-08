# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------

project = 'Musique'
copyright = '2022, Robert Bendun'
author = 'Robert Bendun'

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx_rtd_theme",
    "breathe",
    "recommonmark",
    "exhale"
]

breathe_projects = {
    "musique": "../build/doxygen/xml"
}

breathe_default_project = "musique"

exhale_args = {
    "containmentFolder": "./api",
    "rootFileName": "musique-root.rst",
    "createTreeView": True,
    "doxygenStripFromPath": ".."
}

primary_domain = "cpp"
highlight_language = "cpp"

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']
