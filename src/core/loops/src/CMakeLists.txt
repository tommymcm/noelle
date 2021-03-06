# Sources
set(Srcs 
  MemoryCloningAnalysis.cpp
  Variable.cpp
  AccumulatorOpInfo.cpp
  ControlFlowEquivalence.cpp
  InvariantManager.cpp
  InductionVariables.cpp
  InductionVariable.cpp
  LoopAwareMemDepAnalysis.cpp
  LoopCarriedDependencies.cpp
  LoopGoverningIVAttribution.cpp
  LoopIterationDomainSpaceAnalysis.cpp
  IVStepperUtility.cpp
  SCCAttrs.cpp
  SCCDAGAttrs.cpp
  SCCDAGNormalizer.cpp
  LoopDependenceInfo.cpp
  SCCDAGPartition.cpp
)

# Compilation flags
set_source_files_properties(${Srcs} PROPERTIES COMPILE_FLAGS " -std=c++17 -fPIC")

# Name of the LLVM pass
set(PassName "Loops")

# configure LLVM 
find_package(LLVM REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

message(STATUS "LLVM_DIR IS ${LLVM_CMAKE_DIR}.")

include_directories(${LLVM_INCLUDE_DIRS} 
  ../../basic_utilities/include 
  ../../transformations/include
  ../../dataflow/include
  ../../architecture/include 
  ../../alloc_aa/include 
  ../../pdg/include 
  ../../hotprofiler/include 
  ../../loop_structure/include 
  ../../talkdown/include
  ../include 
  ${CMAKE_INSTALL_PREFIX}/include
  ${CMAKE_INSTALL_PREFIX}/include/svf
  )

# Declare the LLVM pass to compile
add_llvm_library(${PassName} MODULE ${Srcs})
