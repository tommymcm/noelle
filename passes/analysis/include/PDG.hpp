#pragma once

#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/ADT/iterator_range.h"
#include <set>

#include "DGBase.hpp"

using namespace std;
using namespace llvm;

namespace llvm {

  /*
   * Program Dependence Graph.
   */
  class PDG : public DG<Value> {
    public:
      PDG() ;
      ~PDG() ;

      /*
       * Creating Nodes and Edges
       */
      void constructNodes(Module &M);
      void constructControlEdgesForFunction(Function &F, PostDominatorTree &postDomTree);
      /*
       * Creating Program Dependence Subgraphs
       */
      PDG *createFunctionSubgraph(Function &F);
      PDG *createLoopsSubgraph(LoopInfo &LI);
      PDG *createSubgraphFromValues(std::vector<Value *> &valueList);
      
    private:
      void copyEdgesInto(PDG *newPDG, bool linkToExternal = true);
  };
}

