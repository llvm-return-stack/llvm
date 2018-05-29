//===- ReturnStackSanitizer.cpp----------------------------------*- C++ -*-===//
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Author: Philipp Zieris <philipp.zieris@aisec.fraunhofer.de>
//
//===----------------------------------------------------------------------===//
//
// This file implements sanitizers needed for return stack support. Currently
// implemented sanitizers:
//
// * Substitute calls to setjmp, sigsetjmp, longjmp, and siglongjmp with its
//   safe counterparts and insert intrinsics that add unwinding markers onto
//   the return stack.
//
//===----------------------------------------------------------------------===//

#include "llvm/InitializePasses.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ReturnStackSanitizer.h"

using namespace llvm;

#define DEBUG_TYPE "return-stack-sanitizer"

namespace {
  struct ReturnStackSanitizer : public FunctionPass {
  private:
    static uint64_t ReturnStackMarker;

  public:
    static char ID;
    ReturnStackSanitizer() : FunctionPass(ID) {
      initializeReturnStackSanitizerPass(*PassRegistry::getPassRegistry());
    }

    bool runOnFunction(Function &F) override;

    /// Substitutes calls to setjmp, sigsetjmp, longjmp, and siglongjmp with its
    /// safe counterparts and inserts intrinsics that add unwinding markers onto
    /// the return stack.
    bool SetjmpSanitizer(Function &F);
  };
}

uint64_t ReturnStackSanitizer::ReturnStackMarker = 0xfffffffffffffffe;
char ReturnStackSanitizer::ID = 0;
INITIALIZE_PASS(ReturnStackSanitizer, "return-stack-sanitizer",
                "Setjmp/longjmp sanitizer for the return stack protection",
                false, false)

bool ReturnStackSanitizer::runOnFunction(Function &F) {
  return SetjmpSanitizer(F);
}

bool ReturnStackSanitizer::SetjmpSanitizer(Function &F) {

  bool Changed = false;
  Module *M = F.getParent();
  Instruction *EntryInstr = NULL;
  std::vector<Instruction *> ReturnInstrs;
  std::vector<CallInst *> SetjmpCallSites;
  std::vector<CallInst *> LongjmpCallSites;

  if (F.isDeclaration())
    return Changed;

  if (!F.hasFnAttribute(Attribute::ReturnStack))
    return Changed;

  if (!F.empty())
    EntryInstr = F.getEntryBlock().getFirstNonPHI();

  if (!EntryInstr)
    return Changed;

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (isa<ReturnInst>(&I)) {
        ReturnInstrs.push_back(&I);
      } else if (auto CI = dyn_cast<CallInst>(&I)) {
        if (auto CF = CI->getCalledFunction()) {
          if (CF->getName() == "_setjmp" || CF->getName() == "__sigsetjmp")
            SetjmpCallSites.push_back(CI);
          else if (CF->getName() == "longjmp" || CF->getName() == "siglongjmp")
            LongjmpCallSites.push_back(CI);
        }
      }
    }
  }

  if (!SetjmpCallSites.empty() || !LongjmpCallSites.empty())
    Changed = true;

  // Substitute longjmp calls.
  for (auto CI : LongjmpCallSites) {
    auto CF = CI->getCalledFunction();
    if (CF->getName() == "longjmp")
      CF->setName("safe_longjmp");
    else if (CF->getName() == "siglongjmp")
      CF->setName("safe_siglongjmp");
  }

  if (SetjmpCallSites.empty())
    return Changed;

  // Substitute setjmp calls.
  for (auto CI : SetjmpCallSites) {
    auto CF = CI->getCalledFunction();
    if (CF->getName() == "_setjmp")
      CF->setName("_safe_setjmp");
    else if (CF->getName() == "__sigsetjmp")
      CF->setName("__safe_sigsetjmp");
  }

  // Create marker argument for intrinsic call.
  IntegerType *Ty = Type::getIntNTy(F.getContext(),
                                    M->getDataLayout().getPointerSizeInBits());
  ConstantInt *Marker = ConstantInt::get(Ty, ReturnStackMarker);

  // Decrement marker for the next function.
  ReturnStackMarker--;

  // Insert intrinsic call that pushes the marker onto the return stack.
  Function *PushMarkerIntrinsic =
      Intrinsic::getDeclaration(M, Intrinsic::push_return_stack_marker, {Ty});
  CallInst *CI = CallInst::Create(PushMarkerIntrinsic, {Marker});
  CI->insertBefore(EntryInstr);

  // Insert intrinsic calls that pop the marker from the return stack.
  Function *PopMarkerIntrinsic =
      Intrinsic::getDeclaration(M, Intrinsic::pop_return_stack_marker);
  for (auto &I : ReturnInstrs) {
    CallInst *CI = CallInst::Create(PopMarkerIntrinsic);
    CI->insertBefore(I);
  }

  return Changed;
}

FunctionPass *llvm::createReturnStackSanitizerPass() {
  return new ReturnStackSanitizer();
}

