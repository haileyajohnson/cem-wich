
#ifndef bmi_EXPORT_H
#define bmi_EXPORT_H

#ifdef SHARED_EXPORTS_BUILT_AS_STATIC
#  define bmi_EXPORT
#  define BMI_CEM_NO_EXPORT
#else
#  ifndef bmi_EXPORT
#    ifdef bmi_cem_EXPORTS
        /* We are building this library */
#      define bmi_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define bmi_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef BMI_CEM_NO_EXPORT
#    define BMI_CEM_NO_EXPORT 
#  endif
#endif

#ifndef BMI_CEM_DEPRECATED
#  define BMI_CEM_DEPRECATED __declspec(deprecated)
#endif

#ifndef BMI_CEM_DEPRECATED_EXPORT
#  define BMI_CEM_DEPRECATED_EXPORT bmi_EXPORT BMI_CEM_DEPRECATED
#endif

#ifndef BMI_CEM_DEPRECATED_NO_EXPORT
#  define BMI_CEM_DEPRECATED_NO_EXPORT BMI_CEM_NO_EXPORT BMI_CEM_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef BMI_CEM_NO_DEPRECATED
#    define BMI_CEM_NO_DEPRECATED
#  endif
#endif

#endif /* bmi_EXPORT_H */
