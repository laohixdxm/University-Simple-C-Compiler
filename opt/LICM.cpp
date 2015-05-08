//
//  LICM.cpp
//  uscc
//
//  Implements basic loop invariant code motion
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------
#include "Passes.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/ValueTracking.h>
#pragma clang diagnostic pop

using namespace llvm;

namespace uscc
{
namespace opt
{
	
bool LICM::runOnLoop(llvm::Loop *L, llvm::LPPassManager &LPM)
{
    // initialize member variables //
    mChanged = false;
    mCurrLoop = L;
    mLoopInfo = &getAnalysis<LoopInfo>();
    mDomTree = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    
    BasicBlock* BB = mCurrLoop->getHeader();
    hoistPreOrder(mDomTree->getNode(BB));
    
	return mChanged;
}
    
bool LICM::isSafeToHoistInstr(llvm::Instruction *I)
{
    if(mCurrLoop->hasLoopInvariantOperands(I))
    {
        if(isSafeToSpeculativelyExecute(I))
        {
            if(isa<BinaryOperator>(I) || isa<CastInst>(I) || isa<SelectInst>(I) || isa<GetElementPtrInst>(I) || isa<CmpInst>(I))
            {
                return true;
            }
                
        }
    }
    return false;
}

void LICM::hoistInstr(llvm::Instruction *I)
{
    I->moveBefore(mCurrLoop->getLoopPreheader()->getTerminator());
    mChanged = true;
}
    
void LICM::hoistPreOrder(llvm::DomTreeNode * domNode)
{
    BasicBlock* BB = domNode->getBlock();
    if(mLoopInfo->getLoopFor(BB) == mCurrLoop)  // BB is in current loop
    {
        BasicBlock::iterator i = BB->begin();
        llvm::Instruction* currentInstr;
        while(i != BB->end())
        {
            currentInstr = i;
            i++;
            if(isSafeToHoistInstr(currentInstr))
            {
                hoistInstr(currentInstr);
            }
        }
    }
    
    for(auto& child : domNode->getChildren())
    {
        hoistPreOrder(child);
    }
}

void LICM::getAnalysisUsage(AnalysisUsage &Info) const
{
    Info.setPreservesCFG();
    Info.addRequired<DeadBlocks>();
    Info.addRequired<DominatorTreeWrapperPass>();
    Info.addRequired<LoopInfo>();
}
    
	
} // opt
} // uscc

char uscc::opt::LICM::ID = 0;
