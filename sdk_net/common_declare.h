#ifndef __COMMON_DECLARE_H__
#define __COMMON_DECLARE_H__

	#if defined(__GNUC__)
		#define _DEPRECATED_ __attribute__((deprecated))
		#define _FORCE_INLINE_ __attribute__((always_inline))
	#elif defined(_MSC_VER)
		#define _DEPRECATED_
		#define _FORCE_INLINE_ __forceinline
	#else
		#define _DEPRECATED_
		#define _FORCE_INLINE_ inline
	#endif

	#if defined(_MSC_VER)
		#define RS_DLL_IMPORT __declspec(dllimport)
		#define RS_DLL_EXPORT __declspec(dllexport)
		#define RS_DLL_LOCAL
	#elif __GNUC__ >= 4
		#define RS_DLL_IMPORT __attribute__ ((visibility("default")))
		#define RS_DLL_EXPORT __attribute__ ((visibility("default")))
		#define RS_DLL_LOCAL  __attribute__ ((visibility("hidden")))
	#else
		#define RS_DLL_IMPORT
		#define RS_DLL_EXPORT
		#define RS_DLL_LOCALs
	#endif

	// Ignore warnings about import/exports when deriving from std classes.
	#ifdef _MSC_VER
		#pragma warning(disable: 4251)
		#pragma warning(disable: 4275)
	#endif

	#ifdef CLASS_EXPORTS   // we are building a shared lib/dll
		#define RS_CLASS_DECL RS_DLL_EXPORT
	#else                  // we are using shared lib/dll
		#define RS_CLASS_DECL RS_DLL_IMPORT
	#endif


	#ifdef FUNC_EXPORTS    // we are building a shared lib/dll
		#define RS_FUNC_DECL RS_DLL_EXPORT
	#else                  // we are using shared lib/dll
		#define RS_FUNC_DECL RS_DLL_IMPORT
	#endif


#endif
