//
//  DeadBlocks.cpp
//  uscc
//
//  Implements Dead Block Removal optimization pass.
//  This removes blocks from the CFG which are unreachable.
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
#include <llvm/IR/CFG.h>
#include <llvm/ADT/DepthFirstIterator.h>
#pragma clang diagnostic pop
#include <set>
#include <iostream>

using namespace llvm;

namespace uscc
{
namespace opt
{
	
bool DeadBlocks::runOnFunction(Function& F)
{
    BasicBlock* entry = F.begin();
	bool changed = false;
    std::set<BasicBlock*> visitedSet;
    std::set<BasicBlock*> unvisitedSet;
    
    //Perform DFS from entry block
    for (df_ext_iterator<BasicBlock*> dfi = df_ext_begin(entry,visitedSet),
         endi = df_ext_end(entry, visitedSet); dfi != endi; ++dfi)
     {
         
     }

    // Store unvisited blocks
    for(auto& BB : F)
    {
        auto search = visitedSet.find(&BB);
        if(search == visitedSet.end())      // if not in visited set
        {
            unvisitedSet.insert(&BB);   // blocks to be removed...
        }
    }
    
    if(unvisitedSet.size() > 0)
    {
        changed = true;
        for(auto& BB : unvisitedSet)
        {
            for(succ_iterator SI = succ_begin(BB), SE = succ_end(BB);
                SI != SE; SI++)
            {
                SI->removePredecessor(BB);
            }
            BB->eraseFromParent();
        }
    }
   	return changed;
}
	
void DeadBlocks::getAnalysisUsage(AnalysisUsage& Info) const
{
    Info.addRequired<ConstantBranch>();
}
} // opt
} // uscc

char uscc::opt::DeadBlocks::ID = 0;
