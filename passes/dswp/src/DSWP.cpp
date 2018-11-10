#include "DSWP.hpp"

using namespace llvm;

DSWP::DSWP (Module &module, bool forceParallelization, bool enableSCCMerging, Verbosity v)
  :
  ParallelizationTechnique{module, v},
  forceParallelization{forceParallelization},
  enableMergingSCC{enableSCCMerging}
  {

  /*
   * Fetch the function that dispatch the parallelized loop.
   */
  this->workerDispatcher = module.getFunction("stageDispatcher");

  /*
   * Fetch the function that executes a stage.
   */
  auto workerExecuter = module.getFunction("stageExecuter");

  /*
   * Define its signature.
   */
  auto workerArgType = workerExecuter->arg_begin()->getType();
  this->workerType = cast<FunctionType>(cast<PointerType>(workerArgType)->getElementType());

  return ;
}

bool DSWP::canBeAppliedToLoop (
  LoopDependenceInfoForParallelizer *baseLDI,
  Parallelization &par,
  Heuristics *h,
  ScalarEvolution &SE
) const {
  auto LDI = static_cast<DSWPLoopDependenceInfo *>(baseLDI);

  /*
   * Partition the SCCDAG.
   */
  partitionSCCDAG(LDI, h);

  if (this->forceParallelization){
    return true;
  }

  /*
   * Check whether it is worth parallelizing the current loop.
   */
  bool canApply = LDI->partition.subsets.size() > 1;
  if (!canApply && this->verbose > Verbosity::Disabled) {
    errs() << "DSWP:  Not enough TLP can be extracted\n";
    errs() << "DSWP: Exit\n";
  }

  return canApply;
}

bool DSWP::apply (
  LoopDependenceInfoForParallelizer *baseLDI,
  Parallelization &par,
  Heuristics *h,
  ScalarEvolution &SE
) {
  auto LDI = static_cast<DSWPLoopDependenceInfo *>(baseLDI);

  /*
   * Determine DSWP workers (stages)
   */
  generateStagesFromPartitionedSCCs(LDI);
  addRemovableSCCsToStages(LDI);

  /*
   * Collect which queues need to exist between workers
   *
   * NOTE: The trimming of the call graph for all workers is an optimization
   *  that lessens the number of control queues necessary. However,
   *  the algorithm that pops queue values is naive, so the trimming
   *  optimization requires non-control queue information to be collected
   *  prior to its execution. Hence, its weird placement:
   */
  collectDataQueueInfo(LDI, par);
  trimCFGOfStages(LDI);
  collectControlQueueInfo(LDI, par);

  /*
   * Collect information on stages' environments
   */
  std::set<int> nonReducableVars;
  std::set<int> reducableVars;
  for (auto i = 0; i < LDI->environment->envSize(); ++i) nonReducableVars.insert(i);
  initializeEnvironmentBuilder(LDI, nonReducableVars, reducableVars);
  collectLiveInEnvInfo(LDI);
  collectLiveOutEnvInfo(LDI);

  if (this->verbose >= Verbosity::Maximal) {
    printStageSCCs(LDI);
    printStageQueues(LDI);
    printEnv(LDI);
  }

  if (this->verbose > Verbosity::Disabled) {
    errs() << "DSWP:  Create " << this->workers.size() << " pipeline stages\n";
  }

  /*
   * Helper declarations
   */
  LDI->zeroIndexForBaseArray = cast<Value>(ConstantInt::get(par.int64, 0));
  LDI->queueArrayType = ArrayType::get(PointerType::getUnqual(par.int8), LDI->queues.size());
  LDI->stageArrayType = ArrayType::get(PointerType::getUnqual(par.int8), this->workers.size());

  /*
   * Create the pipeline stages (technique workers)
   */
  for (auto i = 0; i < this->workers.size(); ++i) {
    auto worker = (DSWPTechniqueWorker *)this->workers[i];

    /*
     * Add instructions of the current pipeline stage to the worker function
     */
    generateLoopSubsetForStage(LDI, i);

    /*
     * Load pointers of all queues for the current pipeline stage at the function's entry
     * Push/pop queue values between the current pipeline stage and connected ones
     */
    generateLoadsOfQueuePointers(LDI, par, i);
    popValueQueues(LDI, par, i);
    pushValueQueues(LDI, par, i);

    /*
     * Load all loop live-in values at the entry point of the worker.
     * Store final results to loop live-out variables.
     */
    generateCodeToLoadLiveInVariables(LDI, i);
    generateCodeToStoreLiveOutVariables(LDI, i);

    /*
     * Fix the data flow within the parallelized loop by redirecting operands of
     * cloned instructions to refer to the other cloned instructions. Currently,
     * they still refer to the original loop's instructions.
     */
    adjustDataFlowToUseClones(LDI, i);

    /*
     * Add the unconditional branch from the entry basic block to the header of the loop.
     */
    IRBuilder<> entryBuilder(worker->entryBlock);
    entryBuilder.CreateBr(worker->basicBlockClones[LDI->header]);

    /*
     * Add the return instruction at the end of the exit basic block.
     */
    IRBuilder<> exitBuilder(worker->exitBlock);
    exitBuilder.CreateRetVoid();

    /*
     * Inline recursively calls to queues.
     */
    inlineQueueCalls(LDI, i);

    if (this->verbose >= Verbosity::Pipeline) {
      worker->F->print(errs() << "Pipeline stage printout:\n"); errs() << "\n";
    }
  }

  /*
   * Create the whole pipeline by connecting the stages.
   */
  if (this->verbose > Verbosity::Disabled) {
    errs() << "DSWP:  Link pipeline stages\n";
  }
  createPipelineFromStages(LDI, par);

  return true;
}
