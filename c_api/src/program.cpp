#include "taichi/compiler/program.h"
#include "ref_counted.h"

#include "taichi/util/lang_util.h"
#include "taichi/program/program.h"
#include "taichi/rhi/vulkan/vulkan_loader.h"

using taichi::Arch;
using namespace taichi::lang;

extern "C" {

TI_DLL_EXPORT void ti_init_dirs(const char *_compiled_lib_dir, const char *_runtime_tmp_dir) {
    std::string compiled_lib_dir(_compiled_lib_dir);
    std::string runtime_tmp_dir(_runtime_tmp_dir);
    set_taichi_dirs(compiled_lib_dir, runtime_tmp_dir);

    taichi::Logger::get_instance().set_level("trace");
}

TI_DLL_EXPORT void ti_program_materialize_runtime(ti_program program) {
    auto *p = reinterpret_cast<Program *>(program);
    p->materialize_runtime();
}

TI_DLL_EXPORT ti_program ti_program_create(void) {
    assert(taichi::lang::vulkan::is_vulkan_api_available());
    Program *p = new Program(Arch::vulkan);
    RefCounted::new_ref_counted(p);
    return (ti_program)p;
}

TI_DLL_EXPORT void ti_program_add_ref(ti_program program) {
    RefCounted::add_ref(program);
}

TI_DLL_EXPORT void ti_program_release(ti_program program) {
    RefCounted::release(reinterpret_cast<Program*>(program));
}

}
