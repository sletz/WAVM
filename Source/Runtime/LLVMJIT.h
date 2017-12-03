#pragma once

#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "RuntimePrivate.h"
#include "Intrinsics.h"

#ifdef _WIN32
	#pragma warning(push)
	#pragma warning (disable:4267)
	#pragma warning (disable:4800)
	#pragma warning (disable:4291)
	#pragma warning (disable:4244)
	#pragma warning (disable:4351)
	#pragma warning (disable:4065)
	#pragma warning (disable:4624)
	#pragma warning (disable:4245)	// conversion from 'int' to 'unsigned int', signed/unsigned mismatch
	#pragma warning(disable:4146) // unary minus operator applied to unsigned type, result is still unsigned
	#pragma warning(disable:4458) // declaration of 'x' hides class member
	#pragma warning(disable:4510) // default constructor could not be generated
	#pragma warning(disable:4610) // struct can never be instantiated - user defined constructor required
	#pragma warning(disable:4324) // structure was padded due to alignment specifier
	#pragma warning(disable:4702) // unreachable code
#endif

#include "llvm/Config/llvm-config.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolSize.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/Support/Memory.h"
#include <cctype>
#include <string>
#include <vector>

#ifdef _WIN32
	#undef and
	#undef or
	#undef xor
	#pragma warning(pop)
#endif

namespace LLVMJIT
{
	// The global LLVM context.
	extern llvm::LLVMContext context;
	
	// Maps a type ID to the corresponding LLVM type.
	extern llvm::Type* llvmResultTypes[(Uptr)ResultType::num];
	extern llvm::Type* llvmI8Type;
	extern llvm::Type* llvmI16Type;
	extern llvm::Type* llvmI32Type;
	extern llvm::Type* llvmI64Type;
	extern llvm::Type* llvmF32Type;
	extern llvm::Type* llvmF64Type;
	extern llvm::Type* llvmVoidType;
	extern llvm::Type* llvmBoolType;
	extern llvm::Type* llvmI8PtrType;

	extern llvm::Type* llvmI8x16Type;
	extern llvm::Type* llvmI16x8Type;
	extern llvm::Type* llvmI32x4Type;
	extern llvm::Type* llvmI64x2Type;
	extern llvm::Type* llvmF32x4Type;
	extern llvm::Type* llvmF64x2Type;

	#if defined(_WIN64) && ENABLE_EXCEPTION_PROTOTYPE
	extern llvm::Type* llvmExceptionPointersStructType;
	#endif

	// Zero constants of each type.
	extern llvm::Constant* typedZeroConstants[(Uptr)ValueType::num];

	// Converts a WebAssembly type to a LLVM type.
	inline llvm::Type* asLLVMType(ValueType type) { return llvmResultTypes[(Uptr)asResultType(type)]; }
	inline llvm::Type* asLLVMType(ResultType type) { return llvmResultTypes[(Uptr)type]; }

	// Converts a WebAssembly function type to a LLVM type.
	inline llvm::FunctionType* asLLVMType(const FunctionType* functionType)
	{
		auto llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * functionType->parameters.size());
		for(Uptr argIndex = 0;argIndex < functionType->parameters.size();++argIndex)
		{
			llvmArgTypes[argIndex] = asLLVMType(functionType->parameters[argIndex]);
		}
		auto llvmResultType = asLLVMType(functionType->ret);
		return llvm::FunctionType::get(llvmResultType,llvm::ArrayRef<llvm::Type*>(llvmArgTypes,functionType->parameters.size()),false);
	}

	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	inline llvm::ConstantInt* emitLiteral(U32 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI32Type,llvm::APInt(32,(U64)value,false)); }
	inline llvm::ConstantInt* emitLiteral(I32 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI32Type,llvm::APInt(32,(I64)value,false)); }
	inline llvm::ConstantInt* emitLiteral(U64 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI64Type,llvm::APInt(64,value,false)); }
	inline llvm::ConstantInt* emitLiteral(I64 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI64Type,llvm::APInt(64,value,false)); }
	inline llvm::Constant* emitLiteral(F32 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* emitLiteral(F64 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* emitLiteral(bool value) { return llvm::ConstantInt::get(llvmBoolType,llvm::APInt(1,value ? 1 : 0,false)); }
	inline llvm::Constant* emitLiteralPointer(const void* pointer,llvm::Type* type)
	{
		auto pointerInt = llvm::APInt(sizeof(Uptr) == 8 ? 64 : 32,reinterpret_cast<Uptr>(pointer));
		return llvm::Constant::getIntegerValue(type,pointerInt);
	}

	// Functions that map between the symbols used for externally visible functions and the function
	std::string getExternalFunctionName(ModuleInstance* moduleInstance,Uptr functionDefIndex);
	bool getFunctionIndexFromExternalName(const char* externalName,Uptr& outFunctionDefIndex);

	// Emits LLVM IR for a module.
	std::shared_ptr<llvm::Module> emitModule(const IR::Module& module,ModuleInstance* moduleInstance);
	
	// Used to override LLVM's default behavior of looking up unresolved symbols in DLL exports.
	struct NullResolver : llvm::JITSymbolResolver
	{
		static std::shared_ptr<NullResolver> singleton;
		virtual llvm::JITSymbol findSymbol(const std::string& name) override;
		virtual llvm::JITSymbol findSymbolInLogicalDylib(const std::string& name) override;
	};
	
	#ifdef _WIN64
	extern void processSEHTables(
		Uptr imageBaseAddress,
		const llvm::LoadedObjectInfo* loadedObject,
		const llvm::object::SectionRef& pdataSection,const U8* pdataCopy,Uptr pdataNumBytes,
		const llvm::object::SectionRef& xdataSection,const U8* xdataCopy,
		Uptr sehTrampolineAddress
		);
	#endif
}
