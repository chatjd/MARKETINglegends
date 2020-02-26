# ===========================================================================
#   http://www.gnu.org/software/autoconf-archive/ax_gcc_func_attribute.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_GCC_FUNC_ATTRIBUTE(ATTRIBUTE)
#
# DESCRIPTION
#
#   This macro checks if the compiler supports one of GCC's function
#   attributes; many other compilers also provide function attributes with
#   the same syntax. Compiler warnings are used to detect supported
#   attributes as unsupported ones are ignored by default so quieting
#   warnings when using this macro will yield false positives.
#
#   The ATTRIBUTE parameter holds the name of the attribute to be checked.
#
#   If ATTRIBUTE is supported define HAVE_FUNC_ATTRIBUTE_<ATTRIBUTE>.
#
#   The macro caches its result in the ax_cv_have_func_attribute_<attribute>
#   variable.
#
#   The macro currently supports the following function attributes:
#
#    alias
#   