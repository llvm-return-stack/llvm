add_llvm_library(LLVMReturnStackSanitizer
  ReturnStackSanitizer.cpp

  DEPENDS
  intrinsics_gen
)
