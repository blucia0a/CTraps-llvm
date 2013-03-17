#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/Function.h"
#include "llvm/Module.h"

using namespace llvm;


namespace {

  static bool isNonEscapingLocalObject(const Value *V) {
    // If this is a local allocation, check to see if it escapes.
    if (isa<AllocaInst>(V) || isNoAliasCall(V))
      // Set StoreCaptures to True so that we can assume in our callers that the
      // pointer is not the result of a load instruction. Currently
      // PointerMayBeCaptured doesn't have any special analysis for the
      // StoreCaptures=false case; if it did, our callers could be refined to be
      // more precise.
      return !PointerMayBeCaptured(V, false, /*StoreCaptures=*/true);

    // If this is an argument that corresponds to a byval or noalias argument,
    // then it has not escaped before entering the function.  Check if it escapes
    // inside the function.
    if (const Argument *A = dyn_cast<Argument>(V))
      if (A->hasByValAttr() || A->hasNoAliasAttr())
        // Note even if the argument is marked nocapture we still need to check
        // for copies made inside the function. The nocapture attribute only
        // specifies that there are no copies made that outlive the function.
        return !PointerMayBeCaptured(V, false, /*StoreCaptures=*/true);

  return false;
}


  struct CTraps : public FunctionPass {

    static char ID;

    Function *readHook;

    CTraps() : FunctionPass(ID) {}

    virtual bool doInitialization(Module &M){

      ArrayRef<Type *> a;
      FunctionType *f = FunctionType::get( Type::getVoidTy(M.getContext()), a, false);
      readHook = Function::Create(f, GlobalValue::ExternalLinkage, "readHook", &M);

      return true;
    }

    virtual bool doFinalization(Module &M){
      return true;
    }

    virtual void assignPassManager(llvm::PMStack& S, llvm::PassManagerType T){

    }




    virtual bool runOnFunction(Function &func) {

      

      DominatorTree DT;
      DT.getBase().recalculate(func);

      PostDominatorTree PDT;
      PDT.runOnFunction(func);

      errs() << "VCTraps: ";

      errs().write_escaped(func.getName()) << '\n';

      FunctionPassManager fpm(func.getParent());


      std::list<llvm::LoadInst *> loads;
      for(llvm::Function::iterator I = func.begin(), E = func.end(); I != E; ++I){

        for(llvm::BasicBlock::iterator II = I->begin(), EE = I->end(); II != EE; ++II){

          if(llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(II)){

            loads.push_back(load);
            load->getPointerOperand()->print(errs()); errs() << "\n";

          }

        }

      }

      auto li = loads.begin();
      auto le = loads.end();
      for( ; li != le; li++ ){

        LoadInst *load = *li;
        if( isNonEscapingLocalObject(load->getPointerOperand()) ){
          continue;
        }

        /*
        auto li2 = loads.begin();
        auto le2 = loads.end();
        for( ; li2 != le2; li2++ ){

          if(li == li2){ continue; }
          
          if(llvm::Instruction *in1 = llvm::dyn_cast<llvm::Instruction>(*li)){

            if(llvm::Instruction *in2 = llvm::dyn_cast<llvm::Instruction>(*li2)){
              
              if( DT.dominates(in1->getParent(),in2->getParent()) && PDT.dominates(in2->getParent(),in1->getParent()) ){

                errs() << "Control Equivalent Pair: \n";
                in1->print(errs()); errs() << "\n";
                in2->print(errs()); errs() << "\n";

              }

            }

          }

        }*/
        
      }

      return false;
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<AliasAnalysis>();
    }

  };

}

char CTraps::ID = 0;

//INITIALIZE_PASS_BEGIN(CTraps, "ctraps", "CTraps Pass", false, false)

//INITIALIZE_AG_DEPENDENCY(AliasAnalysis)

//INITIALIZE_PASS_END(CTraps, "ctraps", "CTraps Pass", false, false)

static RegisterPass<CTraps> X("ctraps", "CTraps Pass", false, false);

static void registerCTrapsPass(const PassManagerBuilder &, PassManagerBase &PM){
  PM.add(new CTraps());
}

static RegisterStandardPasses RegisterCTrapsPass( PassManagerBuilder::EP_ScalarOptimizerLate, registerCTrapsPass);

