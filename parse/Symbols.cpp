//
//  Symbols.cpp
//  uscc
//
//  Implements the symbol and string tables used for
//  semantic analysis.
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "Symbols.h"
#include "Emitter.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <iostream>
#pragma clang diagnostic pop

using namespace uscc::parse;

llvm::Type* Identifier::llvmType(bool treatArrayAsPtr /* = true */) noexcept
{
	llvm::Type* type = nullptr;
	llvm::LLVMContext& context = llvm::getGlobalContext();
	switch (mType)
	{
		case Type::Char:
			type = llvm::Type::getInt8Ty(context);
			break;
		case Type::Int:
			type = llvm::Type::getInt32Ty(context);
			break;
		case Type::Void:
			type = llvm::Type::getVoidTy(context);
			break;
		case Type::CharArray:
			// Are we treating arrays as pointers?
			if (!treatArrayAsPtr)
			{
				type = llvm::ArrayType::get(llvm::Type::getInt8Ty(context),
											mArrayCount);
			}
			else
			{
				type = llvm::Type::getInt8PtrTy(context);
			}
			break;
		case Type::IntArray:
			// Are we treating arrays as pointers?
			if (!treatArrayAsPtr)
			{
				type = llvm::ArrayType::get(llvm::Type::getInt32Ty(context),
											mArrayCount);
			}
			else
			{
				type = llvm::Type::getInt32PtrTy(context);
			}
			break;
		case Type::Function:
			break;
	}
	
	return type;
}

llvm::Value* Identifier::readFrom(CodeContext& ctx) noexcept
{
	// PA4: Rewrite this entire function
    return ctx.mSSA.readVariable(this,ctx.mBlock);
}

void Identifier::writeTo(CodeContext& ctx, llvm::Value* value) noexcept
{
    return ctx.mSSA.writeVariable(this, ctx.mBlock, value);
}

SymbolTable::SymbolTable() noexcept
{
    mCurrScope = nullptr;
    enterScope();               // enter global scope
    Identifier* func = createIdentifier("@@function");
    func->setType(Type::Function);
    Identifier* var = createIdentifier("@@variable");
    var->setType(Type::Int);
    Identifier* printf = createIdentifier("printf");
    printf->setType(Type::Function);
}

SymbolTable::~SymbolTable() noexcept
{
    delete mCurrScope;    
}

// Returns true if this variable is already declared
// in this scope (ignoring parent scopes).
// Used to prevent redeclaration in the same scope,
// which is disallowed.
bool SymbolTable::isDeclaredInScope(const char* name) const noexcept
{
    Identifier* object = mCurrScope->searchInScope(name);
    if(object == nullptr) return false;
    else return true;
}

// Creates the requested identifier, and returns a pointer
// to it.
// NOTE: If the identifier already exists, nothing will happen.
// This means you should first check with isDeclaredInScope.
Identifier* SymbolTable::createIdentifier(const char* name)
{
    Identifier* ident = new Identifier(name);
    if(!isDeclaredInScope(name))
    {
        mCurrScope->addIdentifier(ident);   // add to current scope table
        return ident;
    }
	else
    {
        return getIdentifier(name);
    }
}

// Returns a pointer to the identifier, if it's found
// Otherwise returns nullptr
Identifier* SymbolTable::getIdentifier(const char* name)
{
    
    Identifier* result = mCurrScope->search(name);
    if(result == nullptr)               // identifier not found
    {
        return nullptr;
    }
    else
    {
        return result;
    }
}

// Enters a new scope, and returns a pointer to this scope table
SymbolTable::ScopeTable* SymbolTable::enterScope()
{
    ScopeTable* ptr = new ScopeTable(mCurrScope);   // param is parent
    mCurrScope = ptr;           // move current scope to the new table
    
    return ptr;
}

// Exits the current scope and moves the current scope back to
// the previous scope table.
void SymbolTable::exitScope()
{
    mCurrScope = mCurrScope->getParent();   // move scope to parent
}

SymbolTable::ScopeTable::ScopeTable(ScopeTable* parent) noexcept
: mParent(parent)
{
    mParent = parent;
    if(parent)
    {
        parent->mChildren.push_back(this);
    }
}

SymbolTable::ScopeTable::~ScopeTable() noexcept
{
    for(auto& scope : mChildren)
    {
        delete scope;
    }
    for(auto& val : mSymbols)
    {
        delete val.second;
    }
}

// Adds the requested identifier to the table
void SymbolTable::ScopeTable::addIdentifier(Identifier* ident)
{
    mSymbols[ident->getName()] = ident;
}

// Searches this scope for an identifier with
// the requested name. Returns nullptr if not found.
Identifier* SymbolTable::ScopeTable::searchInScope(const char* name) noexcept
{
    std::unordered_map<std::string, Identifier*>::const_iterator it = mSymbols.find (name);
    if(it == mSymbols.end())    // identifier not found
    {
        return nullptr;
    }
    else
    {
        return it->second;              // return identifier for this name
    }
}

// Searches this scope first, and if not found searches
// through parent scopes. Returns nullptr if not found.
Identifier* SymbolTable::ScopeTable::search(const char* name) noexcept
{

    Identifier* result;
    Identifier* parent_result;
    result = searchInScope(name);
    ScopeTable* dummy_parent;
    dummy_parent = mParent;         // currents scope's parent
    
    if(searchInScope(name) == nullptr)   // search parent scopes
    {
        while(true)
        {
            parent_result = dummy_parent->searchInScope(name);
            if(parent_result != nullptr)        // found in parent scope
            {
                return parent_result;
            }
            else            // search other parent
            {
                if(dummy_parent->getParent() != nullptr)    // other parent exists
                {
                    dummy_parent = dummy_parent->getParent();
                }
                else break;
            }
        }
    }
    else            // found in currrent scope
    {
        return result;
    }
    return nullptr;             // not found
}

void SymbolTable::ScopeTable::emitIR(CodeContext& ctx)
{
	// The ONLY thing we should alloca now are arrays of a specified size
	// First emit all the symbols in this scope
	for (auto sym : mSymbols)
	{
		Identifier* ident = sym.second;
		llvm::IRBuilder<> build(ctx.mBlock);

		llvm::Value* decl = nullptr;
		
		std::string name = ident->getName();
		
		// It's -1 if it's an array that's passed into a function,
		// in which case we don't allocate it
		if (ident->isArray() && ident->getArrayCount() != -1)
		{
			llvm::Type* type = ident->llvmType(false);
			// Note we pass in "nullptr" for the array size because that's
			// handled by the type
			decl = build.CreateAlloca(type, nullptr, name);
			llvm::cast<llvm::AllocaInst>(decl)->setAlignment(8);
			
			// Make a GEP here so we can access it later on without issue
			std::vector<llvm::Value*> gepIdx;
			gepIdx.push_back(ctx.mZero);
			gepIdx.push_back(ctx.mZero);
			
			decl = build.CreateInBoundsGEP(decl, gepIdx);
			
			// Now write this GEP and save it for this identifier
			ident->writeTo(ctx, decl);
		}
		
	}
	
	// Now emit all the variables in the child scope tables
	for (auto table : mChildren)
	{
		table->emitIR(ctx);
	}
}

StringTable::StringTable() noexcept
{
	
}

StringTable::~StringTable() noexcept
{
	for (auto i : mStrings)
	{
		delete i.second;
	}
}

// Looks up the requested string in the string table
// If it exists, returns the corresponding ConstStr
// Otherwise, constructs a new ConstStr and returns that
ConstStr* StringTable::getString(std::string& val) noexcept
{
	auto iter = mStrings.find(val);
	if (iter != mStrings.end())
	{
		return iter->second;
	}
	else
	{
		ConstStr* newStr = new ConstStr(val);
		mStrings.emplace(val, newStr);
		return newStr;
	}
}

void StringTable::emitIR(CodeContext& ctx) noexcept
{
	for (auto s : mStrings)
	{
		ConstStr* str = s.second;
		// Make the llvm value for this string
		llvm::Constant* strVal = llvm::ConstantDataArray::getString(ctx.mGlobal, str->mText);
		
		// Add this to the global table
		llvm::ArrayType* type = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx.mGlobal),
													 str->mText.size() + 1);
		
		
		llvm::GlobalValue* globVal =
			new llvm::GlobalVariable(*ctx.mModule, type, true,
									 llvm::GlobalValue::LinkageTypes::PrivateLinkage,
									 strVal, ".str");
		// This can be "unnamed" since the address location is not significant
		globVal->setUnnamedAddr(true);
		// Strings are 1-aligned
		//globVal->setAlignment(1);
		
		str->mValue = globVal;
	}
}



