
#ifndef cem_EXPORT_H
#define cem_EXPORT_H

#ifdef bmi_BUILT_AS_STATIC
#  define cem_EXPORT
#  define PY_CEM_NO_EXPORT
#else
#  ifndef cem_EXPORT
#    ifdef py_cem_EXPORTS
        /* We are building this library */
#      define cem_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define cem_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef PY_CEM_NO_EXPORT
#    define PY_CEM_NO_EXPORT 
#  endif
#endif

#ifndef PY_CEM_DEPRECATED
#  define PY_CEM_DEPRECATED __declspec(deprecated)
#endif

#ifndef PY_CEM_DEPRECATED_EXPORT
#  define PY_CEM_DEPRECATED_EXPORT cem_EXPORT PY_CEM_DEPRECATED
#endif

#ifndef PY_CEM_DEPRECATED_NO_EXPORT
#  define PY_CEM_DEPRECATED_NO_EXPORT PY_CEM_NO_EXPORT PY_CEM_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef PY_CEM_NO_DEPRECATED
#    define PY_CEM_NO_DEPRECATED
#  endif
#endif

#endif /* cem_EXPORT_H */
