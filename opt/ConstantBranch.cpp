//
//  ContantBranch.cpp
//  uscc
//
//  Implements Constant Branch Folding opt pass.
//  This converts conditional branches on constants to
//  unconditional branches.
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
#pragma clang diagnostic pop
#include <set>

using namespace llvm;

namespace uscc
{
namespace opt
{
	
bool ConstantBranch::runOnFunction(Function& F)
{
	bool changed = false;
    std::set<BranchInst*> removeSet;
    Function::iterator blockIter = F.begin();
    
    while(blockIter != F.end())
    {
        BasicBlock::iterator instrIter = blockIter->begin();
        while(instrIter != blockIter->end())
        {
            if(BranchInst* br = dyn_cast<BranchInst>(instrIter))
            {
                if(br->isConditional() && isa<ConstantInt>(br->getCondition()))
                   {
                       
                       removeSet.insert(br);
                   }
            }
            instrIter++;
        }
        blockIter++;
    }
    
    if(removeSet.size() > 0)
    {
        changed = true;
        for (std::set<BranchInst*>::iterator br = removeSet.begin();
             br != removeSet.end();
             ++br)
        {
            ConstantInt* condition =dyn_cast<ConstantInt>((*br)->getCondition());
            if(condition->getValue().getBoolValue())
            {
                BasicBlock* leftSuccessor = (*br)->getSuccessor(0); // left successor
                BasicBlock* rightSuccessor = (*br)->getSuccessor(1);
                BranchInst::Create(leftSuccessor, (*br)->getParent());
                rightSuccessor->removePredecessor((*br)->getParent());
            }
            else
            {
                BasicBlock* leftSuccessor = (*br)->getSuccessor(0);
                BasicBlock* rightSuccessor = (*br)->getSuccessor(1);
                BranchInst::Create(rightSuccessor, (*br)->getParent());
                leftSuccessor->removePredecessor((*br)->getParent());
            }
            (*br)->eraseFromParent();       // erase br
        }
    }
	return changed;
}

void ConstantBranch::getAnalysisUsage(AnalysisUsage& Info) const
{
    Info.addRequired<ConstantOps>();    //only execute once the ConstantOps has been executed on function (all constants have been propagated before inspecting branch instruction)
}
	
} // opt
} // uscc

char uscc::opt::ConstantBranch::ID = 0;
