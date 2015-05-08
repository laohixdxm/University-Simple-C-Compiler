//
//  SSABuilder.cpp
//  uscc
//
//  Implements SSABuilder class
//  
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "SSABuilder.h"
#include "../parse/Symbols.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <iostream> 
#pragma clang diagnostic pop

#include <list>

using namespace uscc::opt;
using namespace uscc::parse;
using namespace llvm;

// Called when a new function is started to clear out all the data
void SSABuilder::reset()
{
    for(auto &x : mVarDefs)
    {
        if(x.second)
        {
            x.second->clear();
        }
    }
    for(auto &y : mIncompletePhis)
    {
        if(y.second)
        {
            y.second->clear();
        }
    }
    mVarDefs.clear();
    mIncompletePhis.clear();
    mSealedBlocks.clear();
}

// For a specific variable in a specific basic block, write its value
void SSABuilder::writeVariable(Identifier* var, BasicBlock* block, Value* value)
{
    std::pair<parse::Identifier*, llvm::Value*> varPair(var, value);
    if(mVarDefs[block])     // should not overrite SubMap if it already exists
    {
        SubMap* subVar = mVarDefs[block];
        if(subVar != nullptr)
        {
            // if key already exists, map won't be updated?
            SubMap::iterator it = subVar->find(var);
            if(it != subVar->end())
            {
                // key already exists
                it->second = value; // update to new value
            }
            else
            {
                subVar->insert(varPair);
                mVarDefs[block] = subVar;
            }
        }
        else
        {
            SubMap* subDef =  new SubMap;   // create new submap
            subDef->insert(varPair);
            mVarDefs[block] = subDef;
        }
    }
    else
    {
        SubMap* subDef =  new SubMap;   // create new submap
        subDef->insert(varPair);
        mVarDefs[block] = subDef;
    }
}

// Read the value assigned to the variable in the requested basic block
// Will recursively search predecessor blocks if it was not written in this block
Value* SSABuilder::readVariable(Identifier* var, BasicBlock* block)
{
    if(mVarDefs[block])
    {
        SubMap* subVar = mVarDefs[block];       // get the submap
        if(subVar != nullptr)
        {
            SubMap::iterator got = subVar->find(var);
            if(got != subVar->end())
            {
                return got->second;
            }
        }
    }
    return readVariableRecursive(var, block);
    
}
// This is called to add a new block to the maps
// PA4: Add entry for mVarDefs, mIncompletePhis

void SSABuilder::addBlock(BasicBlock* block, bool isSealed /* = false */)
{
    SubMap* map = new SubMap;
    SubPHI* phis = new SubPHI;
    
    mVarDefs[block] = map;
    mIncompletePhis[block] = phis;
    if(isSealed)
    {
        sealBlock(block);
    }
}

// This is called when a block is "sealed" which means it will not have any
// further predecessors added. It will complete any PHI nodes (if necessary)
void SSABuilder::sealBlock(llvm::BasicBlock* block)
{
    SubPHI* subPhi = mIncompletePhis[block];
    for(auto& s : *subPhi)
    {
        parse::Identifier* var = s.first;
        llvm::PHINode* phi = s.second;
        addPhiOperands(var, phi);
    }
    mSealedBlocks.insert(block);
}

// Recursively search predecessor blocks for a variable
Value* SSABuilder::readVariableRecursive(Identifier* var, BasicBlock* block)
{
	Value* retVal = nullptr;
    PHINode* phi = nullptr;
    std::unordered_set<llvm::BasicBlock*>::const_iterator got = mSealedBlocks.find(block);
    if(got == mSealedBlocks.end())  // incomplete CFGs
    {
        if(block->empty())
        {
            phi = PHINode::Create(var->llvmType(),0,"",block);
        }
        else
        {
            phi = PHINode::Create(var->llvmType(),0,"",&block->front());
        }

        std::pair<parse::Identifier*, llvm::PHINode*> phiPair(var,phi);
        SubPHI* phiMap;
        
        if(mIncompletePhis[block])
        {
            phiMap = mIncompletePhis[block];    // do not create new subPHI if one already exists
            if(phiMap == nullptr)
            {
                phiMap = new SubPHI;
            }
        }
        else    // create new subPHI
        {
            phiMap = new SubPHI;
        }
        
        phiMap->insert(phiPair);
        mIncompletePhis[block] = phiMap;
        writeVariable(var, block, phi);
        return phi;
    }
    else if(block->getSinglePredecessor())      // single predecessor, no phi needed
    {
        retVal = readVariable(var, block->getSinglePredecessor());
        writeVariable(var, block, retVal);
        return retVal;
    }
    else
    {
        if(block->empty())
        {
            phi = PHINode::Create(var->llvmType(),0,"",block);
        }
        else
        {
            phi = PHINode::Create(var->llvmType(),0,"",&block->front());
        }
        writeVariable(var, block, phi);
        retVal = phi;
        retVal = addPhiOperands(var, phi);
        writeVariable(var, block, retVal);
        return retVal;
    }
	return retVal;
}

// Adds phi operands based on predecessors of the containing block
Value* SSABuilder::addPhiOperands(Identifier* var, PHINode* phi)
{
    for (pred_iterator PI = pred_begin(phi->getParent()), E = pred_end(phi->getParent()); PI != E; ++PI)
    {
        //PI is predecessor of a basic block
        phi->addIncoming(readVariable(var, *PI), (*PI));
    }
    return tryRemoveTrivialPhi(phi);
}

// Removes trivial phi nodes
Value* SSABuilder::tryRemoveTrivialPhi(llvm::PHINode* phi)
{
	Value* same = nullptr;
    for(int i = 0; i < phi->getNumIncomingValues(); ++i)
    {
        Value* op = phi->getIncomingValue(i);   //TODO: what should be the parameter?
        if(op == same || op == phi)
        {
            continue;   //unique value or self-reference
        }
        if(same != nullptr)
        {
            return phi;  // not trivial
        }
        same = op;
    }
    
    if(same == nullptr) // unreachable or in start block
    {
        same = UndefValue::get(phi->getType());
    }
    
    // Replace a phi node with "same" value
    phi->replaceAllUsesWith(same);
    // Update the map to use "same", not phi
    SubMap* varDefs = mVarDefs[phi->getParent()];
    for(auto &it : *varDefs)
    {
        if(it.second == phi)
        {
            it.second = same;
        }
    }
    phi->eraseFromParent(); // delete the phi node
    
    for (llvm::PHINode::use_iterator use = phi->use_begin(), E = phi->use_end(); use != E; ++use)
    {
        if(isa<PHINode>(*use))
        {
            tryRemoveTrivialPhi(phi);
        }
    }
    return same;
}
