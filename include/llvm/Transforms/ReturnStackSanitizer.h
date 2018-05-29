//===- ReturnStackSanitizer.cpp----------------------------------*- C++ -*-===//
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Author: Philipp Zieris <philipp.zieris@aisec.fraunhofer.de>
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_RETURNSTACKSANITIZER_H
#define LLVM_TRANSFORMS_RETURNSTACKSANITIZER_H

namespace llvm {

  FunctionPass *createReturnStackSanitizerPass();

} // end namespace llvm

#endif // LLVM_TRANSFORMS_RETURNSTACKSANITIZER_H
