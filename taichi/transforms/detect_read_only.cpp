#include "taichi/ir/ir.h"
#include "taichi/ir/statements.h"
#include "taichi/ir/analysis.h"
#include "taichi/ir/visitors.h"
#include "taichi/ir/transforms.h"

#include <algorithm>

TLANG_NAMESPACE_BEGIN

namespace irpass {

namespace {

void detect_read_only_in_task(OffloadedStmt *offload) {
  auto accessed = irpass::analysis::gather_snode_read_writes(offload);
  for (auto snode : accessed.first) {
    if (accessed.second.count(snode) == 0) {
      // read-only SNode
      offload->mem_access_opt.add_flag(snode, SNodeAccessFlag::read_only);
    }
  }
}

class ExternalPtrAccessVisitor : public BasicStmtVisitor {
 private:
  std::unordered_map<int, int> &map;

 public:
  using BasicStmtVisitor::visit;

  ExternalPtrAccessVisitor(std::unordered_map<int, int> &map)
      : map(map), BasicStmtVisitor() {
  }

  void visit(GlobalLoadStmt *stmt) {
    if (stmt->src && stmt->src->is<ExternalPtrStmt>()) {
      ExternalPtrStmt *src = stmt->src->cast<ExternalPtrStmt>();
      ArgLoadStmt *arg = src->base_ptrs.data[0]->cast<ArgLoadStmt>();
      if (map.find(arg->arg_id) != map.end()) {
        map[arg->arg_id] = int(map[arg->arg_id]) | int(ExternalPtrAccess::READ);
      } else {
        map[arg->arg_id] = int(ExternalPtrAccess::READ);
      }
    }
  }

  void visit(GlobalStoreStmt *stmt) {
    if (stmt->dest && stmt->dest->is<ExternalPtrStmt>()) {
      ExternalPtrStmt *dst = stmt->dest->cast<ExternalPtrStmt>();
      ArgLoadStmt *arg = dst->base_ptrs.data[0]->cast<ArgLoadStmt>();
      if (map.find(arg->arg_id) != map.end()) {
        map[arg->arg_id] =
            int(map[arg->arg_id]) | int(ExternalPtrAccess::WRITE);
      } else {
        map[arg->arg_id] = int(ExternalPtrAccess::WRITE);
      }
    }
  }
};

}  // namespace

void detect_read_only(IRNode *root) {
  if (root->is<Block>()) {
    for (auto &offload : root->as<Block>()->statements) {
      detect_read_only_in_task(offload->as<OffloadedStmt>());
    }
  } else {
    detect_read_only_in_task(root->as<OffloadedStmt>());
  }
}

std::unordered_map<int, int> detect_external_ptr_access_in_task(
    OffloadedStmt *offload) {
  std::unordered_map<int, int> map;
  ExternalPtrAccessVisitor v(map);
  offload->accept(&v);
  return map;
}

}  // namespace irpass

TLANG_NAMESPACE_END
