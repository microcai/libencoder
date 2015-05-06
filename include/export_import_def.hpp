
#pragma once

//
// Dynamic shared object (DSO) and dynamic-link library (DLL) support
//
#if __GNUC__ >= 4
#  if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__)
     // All Win32 development environments, including 64-bit Windows and MinGW, define
     // _WIN32 or one of its variant spellings. Note that Cygwin is a POSIX environment,
     // so does not define _WIN32 or its variants.
#    define LIB_SYMBOL_EXPORT __attribute__((__dllexport__))
#    define LIB_SYMBOL_IMPORT __attribute__((__dllimport__))
#  else
#    define LIB_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#    define LIB_SYMBOL_IMPORT
#  endif
#  define LIB_SYMBOL_VISIBLE __attribute__((__visibility__("default")))
#endif

#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__)
# ifndef LIB_SYMBOL_EXPORT
#  define LIB_SYMBOL_EXPORT __declspec(dllexport)
#  define LIB_SYMBOL_IMPORT __declspec(dllimport)
# endif
#endif

# ifndef LIB_SYMBOL_EXPORT
#  define LIB_SYMBOL_EXPORT
#  define LIB_SYMBOL_IMPORT
# endif

