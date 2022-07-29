#ifndef __INCLUDE_C_API_TAICHI_PROGRAM_H__
#define __INCLUDE_C_API_TAICHI_PROGRAM_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "taichi_platform.h"

typedef struct ti_program_class *ti_program;

/**
 * Initialize the dirs of taichi library
 * 
 * @param compiled_lib_dir 
 * @param runtime_lib_dir 
 */
TI_DLL_EXPORT void TI_API_CALL ti_init_dirs(const char *compiled_lib_dir, const char *runtime_tmp_dir);

/**
 * Create a new Taichi Program.
 *
 * @return The program that's created. Returns NULL when failed.
 */
TI_DLL_EXPORT ti_program TI_API_CALL ti_program_create(void);

/**
 * Adds a reference to a Taichi Program object.
 *
 * @param program The program object to add a reference to.
 */
TI_DLL_EXPORT void TI_API_CALL ti_program_add_ref(ti_program program);

/**
 * Releases a reference to a Taichi Program object.
 *
 * @param program The program object to release a reference to.
 */
TI_DLL_EXPORT void TI_API_CALL ti_program_release(ti_program program);

/**
 * Materialize the device runtime of a Taichi Program object.
 *
 * @param program The program object.
 */
TI_DLL_EXPORT void TI_API_CALL ti_program_materialize_runtime(ti_program program);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __INCLUDE_C_API_TAICHI_PROGRAM_H__