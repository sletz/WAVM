/************************************************************************
 ************************************************************************
 Copyright (C) 2017 GRAME, Centre National de Creation Musicale
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 2.1 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
 ************************************************************************
 ************************************************************************/

#include <iostream>
#include <sstream>
#include <cmath>

#include <string.h>

#include "Runtime/Intrinsics.h"
#include "IR/Module.h"
#include "Runtime/Linker.h"
#include "CLI.h"

#include "wasm_dsp_aux.hh"

using namespace IR;
using namespace Runtime;

// Module imported mathematical functions

/*
// Integer version
DEFINE_INTRINSIC_FUNCTION1(env,abs,abs,i32,i32,value) { return std::abs(value); }

// Float (= f32) version
DEFINE_INTRINSIC_FUNCTION1(env,acos,acos,f32,f32,value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,asin,asin,f32,f32,value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,atan,atan,f32,f32,value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION2(env,atan2,atan2,f32,f32,left,f32,right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,cos,cos,f32,f32,value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,exp,exp,f32,f32,value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION2(env,fmod,fmod,f32,f32,left,f32,right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,log,log,f32,f32,value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION1(env,log10,log10,f32,f32,value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION2(env,pow,pow,f32,f32,left,f32,right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION2(env,remainder,remainder,f32,f32,left,f32,right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,round,round,f32,f32,value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION1(env,sin,sin,f32,f32,value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,tan,tan,f32,f32,value) { return std::tan(value); }

// Double (= f64) version
DEFINE_INTRINSIC_FUNCTION1(env,acos,acos,f64,f64,value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,asin,asin,f64,f64,value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,atan,atan,f64,f64,value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION2(env,atan2,atan2,f64,f64,left,f64,right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,cos,cos,f64,f64,value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,exp,exp,f64,f64,value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION2(env,fmod,fmod,f64,f64,left,f64,right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,log,log,f64,f64,value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION1(env,log10,log10,f64,f64,value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION2(env,pow,pow,f64,f64,left,f64,right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION2(env,remainder,remainder,f64,f64,left,f64,right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,round,round,f64,f64,value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION1(env,sin,sin,f64,f64,value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,tan,tan,f64,f64,value) { return std::tan(value); }
*/

DEFINE_INTRINSIC_FUNCTION3(env,_memset,_memset,i32,i32,v1,i32,v2,i32,v3)
{
    //return reinterpret_cast<int>(memset(reinterpret_cast<void *>(v1),static_cast<int>(v2),static_cast<size_t>(v3)));
    return 0;
}

// Integer version

// Float (= f32) version
DEFINE_INTRINSIC_FUNCTION1(env,_acosf,_acosf,f32,f32,value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_asinf,_asinf,f32,f32,value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_atanf,_atanf,f32,f32,value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION2(env,_atan2f,_atan2f,f32,f32,left,f32,right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,_cosf,_cosf,f32,f32,value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_expf,_expf,f32,f32,value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION2(env,_fmodf,_fmodf,f32,f32,left,f32,right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,_logf,_logf,f32,f32,value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_log10f,_log10f,f32,f32,value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION2(env,_powf,_powf,f32,f32,left,f32,right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION2(env,_remainderf,_remainderf,f32,f32,left,f32,right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,_roundf,_roundf,f32,f32,value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_sinf,_sinf,f32,f32,value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_tanf,_tanf,f32,f32,value) { return std::tan(value); }

// Double (= f64) version
DEFINE_INTRINSIC_FUNCTION1(env,_acos,_acos,f64,f64,value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_asin,_asin,f64,f64,value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_atan,_atan,f64,f64,value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION2(env,_atan2,_atan2,f64,f64,left,f64,right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,_cos,_cos,f64,f64,value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_exp,_exp,f64,f64,value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION2(env,_fmod,_fmod,f64,f64,left,f64,right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,_log,_log,f64,f64,value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_log10,_log10,f64,f64,value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION2(env,_pow,_pow,f64,f64,left,f64,right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION2(env,_remainder,_remainder,f64,f64,left,f64,right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION1(env,_round,_round,f64,f64,value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_sin,_sin,f64,f64,value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION1(env,_tan,_tan,f64,f64,value) { return std::tan(value); }

// Tools for Module import

struct RootResolver : Resolver
{
    std::map<std::string,Resolver*> moduleNameToResolverMap;
    
    bool resolve(const std::string& moduleName,const std::string& exportName,ObjectType type,ObjectInstance*& outObject) override
    {
        // Try to resolve an intrinsic first.
        if(IntrinsicResolver::singleton.resolve(moduleName,exportName,type,outObject)) { return true; }
        
        // Then look for a named module.
        auto namedResolverIt = moduleNameToResolverMap.find(moduleName);
        if(namedResolverIt != moduleNameToResolverMap.end())
        {
            return namedResolverIt->second->resolve(moduleName,exportName,type,outObject);
        }
        
        // Finally, stub in missing function imports.
        if(type.kind == ObjectKind::function)
        {
            // Generate a function body that just uses the unreachable op to fault if called.
            Serialization::ArrayOutputStream codeStream;
            OperatorEncoderStream encoder(codeStream);
            encoder.unreachable();
            encoder.end();
            
            // Generate a module for the stub function.
            Module stubModule;
            DisassemblyNames stubModuleNames;
            stubModule.types.push_back(asFunctionType(type));
            stubModule.functions.defs.push_back({{0},{},std::move(codeStream.getBytes()),{}});
            stubModule.exports.push_back({"importStub",ObjectKind::function,0});
            stubModuleNames.functions.push_back({std::string(moduleName) + "." + exportName,{}});
            IR::setDisassemblyNames(stubModule,stubModuleNames);
            IR::validateDefinitions(stubModule);
            
            // Instantiate the module and return the stub function instance.
            auto stubModuleInstance = instantiateModule(stubModule,{});
            outObject = getInstanceExport(stubModuleInstance,"importStub");
            Log::printf(Log::Category::error,"Generated stub for missing function import %s.%s : %s\n",moduleName.c_str(),exportName.c_str(),asString(type).c_str());
            return true;
        }
        else if(type.kind == ObjectKind::memory)
        {
            outObject = asObject(Runtime::createMemory(asMemoryType(type)));
            Log::printf(Log::Category::error,"Generated stub for missing memory import %s.%s : %s\n",moduleName.c_str(),exportName.c_str(),asString(type).c_str());
            return true;
        }
        else if(type.kind == ObjectKind::table)
        {
            outObject = asObject(Runtime::createTable(asTableType(type)));
            Log::printf(Log::Category::error,"Generated stub for missing table import %s.%s : %s\n",moduleName.c_str(),exportName.c_str(),asString(type).c_str());
            return true;
        }
        else if(type.kind == ObjectKind::global)
        {
            outObject = asObject(Runtime::createGlobal(asGlobalType(type),Runtime::Value(asGlobalType(type).valueType,Runtime::UntaggedValue())));
            Log::printf(Log::Category::error,"Generated stub for missing global import %s.%s : %s\n",moduleName.c_str(),exportName.c_str(),asString(type).c_str());
            return true;
        }
        
        return false;
    }
};

// Public DSP API

Module* wasm_dsp::gFactoryModule = nullptr;

bool wasm_dsp::init(const char* filename)
{
    if (!gFactoryModule) {
        gFactoryModule = new Module();
        if (!loadModule(filename, *gFactoryModule)) {
            std::cerr << "Failed to load module" << std::endl;
            return false;
        }
    }
    return true;
}

wasm_dsp::wasm_dsp(Module* module)
{
    RootResolver rootResolver;
    LinkResult linkResult = linkModule(*module, rootResolver);
    
    if(!linkResult.success)
    {
        std::cerr << "Failed to link module:" << std::endl;
        for(auto& missingImport : linkResult.missingImports)
        {
            std::cerr << "Missing import: module=\"" << missingImport.moduleName
            << "\" export=\"" << missingImport.exportName
            << "\" type=\"" << asString(missingImport.type) << "\"" << std::endl;
        }
        throw std::bad_alloc();
    }
    
    fModuleInstance = instantiateModule(*module, std::move(linkResult.resolvedImports));
    if (!fModuleInstance) {
        std::cerr << "Failed to instantiateModule" << std::endl;
        throw std::bad_alloc();
    }
    
    fGetNumInputs = asFunctionNullable(getInstanceExport(fModuleInstance, "getNumInputs"));
    if (!fGetNumInputs) {
        std::cerr << "Error : module is not compiled with Faust...\n";
        throw std::bad_alloc();
    }
    fGetNumOutputs = asFunctionNullable(getInstanceExport(fModuleInstance, "getNumOutputs"));
    fGetSampleRate = asFunctionNullable(getInstanceExport(fModuleInstance, "getSampleRate"));
    fInit = asFunctionNullable(getInstanceExport(fModuleInstance, "init"));
    fInstanceInit = asFunctionNullable(getInstanceExport(fModuleInstance, "instanceInit"));
    fInstanceConstants = asFunctionNullable(getInstanceExport(fModuleInstance, "instanceConstants"));
    fInstanceResetUI = asFunctionNullable(getInstanceExport(fModuleInstance, "instanceResetUserInterface"));
    fInstanceClear = asFunctionNullable(getInstanceExport(fModuleInstance, "instanceClear"));
    fSetParamValue = asFunctionNullable(getInstanceExport(fModuleInstance, "setParamValue"));
    fGetParamValue = asFunctionNullable(getInstanceExport(fModuleInstance, "getParamValue"));
    fCompute = asFunctionNullable(getInstanceExport(fModuleInstance, "compute"));
    
    MemoryInstance* memory = getDefaultMemory(fModuleInstance);
    
    // JSON is located at offset 0 in the memory segment
    fDecoder = new JSONUIDecoder1(getJSON(getMemoryBaseAddress(memory)), memory);
    
    int ptr_size = sizeof(FAUSTFLOAT*);
    int sample_size = sizeof(FAUSTFLOAT);
    int buffer_size = 4096; // Max
    
    fInputs = new FAUSTFLOAT*[fDecoder->fNumInputs];
    fOutputs = new FAUSTFLOAT*[fDecoder->fNumOutputs];
    
    // DSP is placed first with index 0. Audio buffer start at the end of DSP.
    int audio_heap_ptr = fDecoder->fDSPSize;
    
    // Setup pointers offset
    int audio_heap_ptr_inputs = audio_heap_ptr;
    int audio_heap_ptr_outputs = audio_heap_ptr_inputs + (fDecoder->fNumInputs * ptr_size);
    
    // Setup buffer offset
    int audio_heap_inputs = audio_heap_ptr_outputs + (fDecoder->fNumOutputs * ptr_size);
    int audio_heap_outputs = audio_heap_inputs + (fDecoder->fNumInputs * buffer_size * sample_size);
    
    if (fDecoder->fNumInputs > 0) {
        
        fWasmInputs = audio_heap_ptr_inputs;
        int* HEAP32 = reinterpret_cast<int*>(getValidatedMemoryOffsetRange(memory, audio_heap_ptr_inputs, 0));
        FAUSTFLOAT* HEAPF32 = reinterpret_cast<FAUSTFLOAT*>(getValidatedMemoryOffsetRange(memory, audio_heap_inputs, 0));
        
        for (int i = 0; i < fDecoder->fNumInputs; i++) {
            // Setup input buffer indexes for wasm side
            HEAP32[i] = audio_heap_inputs + (buffer_size * sample_size * i);
            // Setup input buffer pointers for runtime side
            fInputs[i] = HEAPF32 + (buffer_size * i);
        }
    }
    
    if (fDecoder->fNumOutputs > 0) {
        
        fWasmOutputs = audio_heap_ptr_outputs;
        int* HEAP32 = reinterpret_cast<int*>(getValidatedMemoryOffsetRange(memory, audio_heap_ptr_outputs, 0));
        FAUSTFLOAT* HEAPF32 = reinterpret_cast<FAUSTFLOAT*>(getValidatedMemoryOffsetRange(memory, audio_heap_outputs, 0));
        
        for (int i = 0; i < fDecoder->fNumOutputs; i++) {
            // Setup output buffer indexes for wasm side
            HEAP32[i] = audio_heap_outputs + (buffer_size * sample_size * i);
            // Setup output buffer pointers for runtime side
            fOutputs[i] =  HEAPF32 + (buffer_size * i);
        }
    }
    
    std::cout << "Libfaust version: " << fDecoder->fVersion << std::endl;
    std::cout << "Compilation options: " << fDecoder->fOptions << std::endl;
}

wasm_dsp::~wasm_dsp()
{
    delete fDecoder;
    delete [] fInputs;
    delete [] fOutputs;
}

int wasm_dsp::getNumInputs()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    auto functionResult = invokeFunction(fGetNumInputs, invokeArgs);
    return functionResult.i32;
}
        
int wasm_dsp::getNumOutputs()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    auto functionResult = invokeFunction(fGetNumOutputs, invokeArgs);
    return functionResult.i32;
}

void wasm_dsp::buildUserInterface(UI* ui_interface)
{
    fDecoder->buildUserInterface(ui_interface);
}

int wasm_dsp::getSampleRate()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    auto functionResult = invokeFunction(fGetSampleRate, invokeArgs);
    return functionResult.i32;
}

void wasm_dsp::init(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunction(fInit, invokeArgs);
}

void wasm_dsp::instanceInit(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunction(fInstanceInit, invokeArgs);
}

void wasm_dsp::instanceConstants(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunction(fInstanceConstants, invokeArgs);
}

void wasm_dsp::instanceResetUserInterface()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    invokeFunction(fInstanceResetUI, invokeArgs);
}

void wasm_dsp::instanceClear()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    invokeFunction(fInstanceClear, invokeArgs);
}

wasm_dsp* wasm_dsp::clone()
{
    return new wasm_dsp(wasm_dsp::gFactoryModule);
}

void wasm_dsp::metadata(Meta* m)
{
    fDecoder->metadata(m);
}

void wasm_dsp::compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs)
{
    try
    {
        // Copy audio inputs
        for (int i = 0; i < fDecoder->fNumInputs; i++) {
            memcpy(fInputs[i], inputs[i], sizeof(FAUSTFLOAT) * count);
        }
        
        // Call wasm compute
        computeAux(count);
        
        // Copy audio outputs
        for (int i = 0; i < fDecoder->fNumOutputs; i++) {
            memcpy(outputs[i], fOutputs[i], sizeof(FAUSTFLOAT) * count);
        }
        
    } catch(Runtime::Exception exception) {
        //std::cerr << "Runtime exception: " << describeExceptionCause(exception.cause) << std::endl;
        //for (auto calledFunction : exception.callStack) { std::cerr << " " << calledFunction << std::endl; }
    }
}

