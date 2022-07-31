import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:path/path.dart' as p;
import 'ticore_bindings.dart' as ticore;

import 'dart:io' show Platform;

final compiledLibDir = p.join(Directory.current.path, "../../python/taichi/_lib/runtime");
final runtimeTmpDir = p.join(Directory.current.path, "tmp");

Pointer<ticore.ti_program_class> ?program;
ticore.TaichiCore ?ticoreImpl;

void init() {
  // Load C library
  var libraryName = "libtaichi_c_api.so";
  if (Platform.isWindows) {
    libraryName = "taichi_c_api.dll";
  } else if (Platform.isMacOS) {
    libraryName = "libtaichi_c_api.dylib";
  }

  ticoreImpl = ticore.TaichiCore(DynamicLibrary.open(p.join(Directory.current.path, "../../build", libraryName)));

  // Initialize Taichi globals
  final compiledLibDirCStr = compiledLibDir.toNativeUtf8().cast<Char>();
  final runtimeTmpDirCStr = runtimeTmpDir.toNativeUtf8().cast<Char>();
  ticoreImpl!.ti_init_dirs(compiledLibDirCStr, runtimeTmpDirCStr);
  malloc.free(compiledLibDirCStr);
  malloc.free(runtimeTmpDirCStr);

  // Create taichi::lang::Program
  program = ticoreImpl!.ti_program_create(ticore.TiArch.TI_ARCH_METAL);
  ticoreImpl!.ti_program_materialize_runtime(program!);
}

void terminate() {
  // Clean-up
  ticoreImpl!.ti_program_release(program!);
}