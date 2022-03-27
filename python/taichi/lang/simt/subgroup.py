from taichi._lib import core as _ti_core
from taichi.lang import expr

def barrier():
    # TODO
    pass

def memory_barrier():
    # TODO
    pass

def elect():
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupElect", 
             expr.make_expr_group(), False)) 

def all_true(cond):
    # TODO
    pass

def any_true(cond):
    # TODO
    pass

def all_equal(value):
    # TODO
    pass

def broadcast_first(value):
    # TODO
    pass

def reduce_add(value):
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupAdd", 
             expr.make_expr_group(value), False)) 

def reduce_mul(value):
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupMul", 
             expr.make_expr_group(value), False)) 

def reduce_min(value):
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupMin", 
             expr.make_expr_group(value), False)) 

def reduce_max(value):
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupMax", 
             expr.make_expr_group(value), False)) 

def reduce_and(value):
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupAnd", 
             expr.make_expr_group(value), False)) 

def reduce_or(value):
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupOr", 
             expr.make_expr_group(value), False)) 

def reduce_xor(value):
    return expr.Expr( 
         _ti_core.insert_internal_func_call( 
             "subgroupXor", 
             expr.make_expr_group(value), False)) 

def inclusive_add(value):
    # TODO
    pass

def inclusive_mul(value):
    # TODO
    pass

def inclusive_min(value):
    # TODO
    pass

def inclusive_max(value):
    # TODO
    pass

def inclusive_and(value):
    # TODO
    pass

def inclusive_or(value):
    # TODO
    pass

def inclusive_xor(value):
    # TODO
    pass

def exclusive_add(value):
    # TODO
    pass

def exclusive_mul(value):
    # TODO
    pass

def exclusive_min(value):
    # TODO
    pass

def exclusive_max(value):
    # TODO
    pass

def exclusive_and(value):
    # TODO
    pass

def exclusive_or(value):
    # TODO
    pass

def exclusive_xor(value):
    # TODO
    pass

def shuffle(value, index):
    # TODO
    pass

def shuffle_xor(value, mask):
    # TODO
    pass

def shuffle_up(value, offset):
    # TODO
    pass

def shuffle_down(value, offset):
    # TODO
    pass

__all__ = [
    'barrier',
    'memory_barrier',
    'elect',
    'all_true',
    'any_true',
    'all_equal',
    'broadcast_first',
    'reduce_add',
    'reduce_mul',
    'reduce_min',
    'reduce_max',
    'reduce_and',
    'reduce_or',
    'reduce_xor',
    'inclusive_add',
    'inclusive_mul',
    'inclusive_min',
    'inclusive_max',
    'inclusive_and',
    'inclusive_or',
    'inclusive_xor',
    'exclusive_add',
    'exclusive_mul',
    'exclusive_min',
    'exclusive_max',
    'exclusive_and',
    'exclusive_or',
    'exclusive_xor',
    'shuffle',
    'shuffle_xor',
    'shuffle_up',
    'shuffle_down'
]
