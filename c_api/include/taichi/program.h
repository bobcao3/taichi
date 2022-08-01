#ifndef __INCLUDE_C_API_TAICHI_PROGRAM_H__
#define __INCLUDE_C_API_TAICHI_PROGRAM_H__

#include "taichi_platform.h"
#include "taichi_core.h"
#include "frontend_ir.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct ti_program_class *ti_program;
typedef struct ti_kernel_class *ti_kernel;

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
 * @param arch The back-end architecture of the program.
 * @return The program that's created. Returns NULL when failed.
 */
TI_DLL_EXPORT ti_program TI_API_CALL ti_program_create(TiArch arch);

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

/**
 * Create a Taichi Kernel for a Taichi Program
 * 
 * @param ast_node The AST root node of the kernel, must be a ti_block.
 * @param name (Optional, can be NULL) The name of the kernel
 */
TI_DLL_EXPORT ti_kernel TI_API_CALL ti_kernel_create(ti_program program,
                                                     ti_block ast_node,
                                                     const char *name);

/**
 * Compile a Taichi Kernel targeting program
 *
 * @param program The target Taichi Program.
 * @param kernel The kernel to be compiled.
 * @return The handle of the compiled kernel.
 */
TI_DLL_EXPORT int TI_API_CALL ti_program_compile_kernel(ti_program program,
                                                        ti_kernel kernel);

/**
 * Launch a compiled taichi kernel
 *
 * @param id The id/handle of the kernel to be launched.
 * @param runtime_context The RuntimeContext for the kernel.
 */
TI_DLL_EXPORT void TI_API_CALL ti_program_launch_kernel(int id, void *runtime_context);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __INCLUDE_C_API_TAICHI_PROGRAM_H__