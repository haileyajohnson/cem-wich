
#ifndef cem_EXPORT_H
#define cem_EXPORT_H

#ifdef TEST_CEM_STATIC_DEFINE
#  define cem_EXPORT
#  define TEST_CEM_NO_EXPORT
#else
#  ifndef cem_EXPORT
#    ifdef test_cem_EXPORTS
        /* We are building this library */
#      define cem_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define cem_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef TEST_CEM_NO_EXPORT
#    define TEST_CEM_NO_EXPORT 
#  endif
#endif

#ifndef TEST_CEM_DEPRECATED
#  define TEST_CEM_DEPRECATED __declspec(deprecated)
#endif

#ifndef TEST_CEM_DEPRECATED_EXPORT
#  define TEST_CEM_DEPRECATED_EXPORT cem_EXPORT TEST_CEM_DEPRECATED
#endif

#ifndef TEST_CEM_DEPRECATED_NO_EXPORT
#  define TEST_CEM_DEPRECATED_NO_EXPORT TEST_CEM_NO_EXPORT TEST_CEM_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef TEST_CEM_NO_DEPRECATED
#    define TEST_CEM_NO_DEPRECATED
#  endif
#endif

#endif /* cem_EXPORT_H */
