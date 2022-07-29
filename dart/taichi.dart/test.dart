import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:path/path.dart' as p;
import 'ticore_bindings.dart' as ticore;

final compiledLibDir = p.join(Directory.current.path, "../../python/taichi/_lib/runtime");
final runtimeTmpDir = p.join(Directory.current.path, "tmp");
final ticoreImpl = ticore.TaichiCore(DynamicLibrary.open(p.join(Directory.current.path, "../../build/libtaichi_c_api.dylib")));

void main() {
  // Initialize Taichi globals
  final compiledLibDirCStr = compiledLibDir.toNativeUtf8().cast<Char>();
  final runtimeTmpDirCStr = runtimeTmpDir.toNativeUtf8().cast<Char>();
  ticoreImpl.ti_init_dirs(compiledLibDirCStr, runtimeTmpDirCStr);
  malloc.free(compiledLibDirCStr);
  malloc.free(runtimeTmpDirCStr);

  // Create taichi::lang::Program
  final program = ticoreImpl.ti_program_create();
  ticoreImpl.ti_program_materialize_runtime(program);
}
