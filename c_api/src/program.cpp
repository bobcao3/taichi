#include "taichi/program.h"
#include "ref_counted.h"

#include "taichi/util/lang_util.h"
#include "taichi/program/program.h"
#include "taichi/rhi/vulkan/vulkan_loader.h"

using taichi::Arch;
using namespace taichi::lang;

namespace {

static std::mutex kernels_lock;
static std::vector<FunctionType> registered_kernels;

}

extern "C" {

void ti_init_dirs(const char *_compiled_lib_dir, const char *_runtime_tmp_dir) {
    std::string compiled_lib_dir(_compiled_lib_dir);
    std::string runtime_tmp_dir(_runtime_tmp_dir);
    set_taichi_dirs(compiled_lib_dir, runtime_tmp_dir);

    taichi::Logger::get_instance().set_level("trace");
}

void ti_program_materialize_runtime(ti_program program) {
    auto *p = reinterpret_cast<Program *>(program);
    p->materialize_runtime();
}

ti_program ti_program_create(TiArch arch) {
    Program *p = new Program(Arch(arch));
    RefCounted::new_ref_counted(p);
    return (ti_program)p;
}

void ti_program_add_ref(ti_program program) {
    RefCounted::add_ref(program);
}

void ti_program_release(ti_program program) {
    RefCounted::release(reinterpret_cast<Program*>(program));
}

ti_kernel ti_kernel_create(ti_program program, ti_block ast_node, const char *name) {
  Block *block_node_cptr = reinterpret_cast<Block *>(ast_node);
  std::unique_ptr<IRNode> block = std::unique_ptr<Block>(block_node_cptr);
  
  Kernel *k = new Kernel(*reinterpret_cast<Program *>(program), std::move(block),
                         name ? std::string(name) : "", AutodiffMode::kNone, true);
  RefCounted::new_ref_counted(k);

  return ti_kernel(k);
}

int ti_program_compile_kernel(ti_program program, ti_kernel kernel) {
  auto f = reinterpret_cast<Program *>(program)->compile(*reinterpret_cast<Kernel *>(kernel));
  
  std::lock_guard<std::mutex> lg(kernels_lock);
  int id = int(registered_kernels.size());
  registered_kernels.push_back(f);

  return id;
}

void ti_program_launch_kernel(int id, void *runtime_context) {
  registered_kernels[id](*reinterpret_cast<RuntimeContext *>(runtime_context));
}

}
