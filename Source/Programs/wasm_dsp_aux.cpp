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

DEFINE_INTRINSIC_MODULE(env)

struct RootResolver : Resolver
{
    Compartment* compartment;
    std::map<std::string,ModuleInstance*> moduleNameToInstanceMap;
    
    RootResolver(Compartment* inCompartment): compartment(inCompartment)
    {
        // Steph : 19/04/18
        moduleNameToInstanceMap["env"] = instantiateModule(compartment,INTRINSIC_MODULE_REF(env), "env");
    }
    
    bool resolve(const std::string& moduleName,const std::string& exportName,ObjectType type,Object*& outObject) override
    {
        auto namedInstanceIt = moduleNameToInstanceMap.find(moduleName);
        if(namedInstanceIt != moduleNameToInstanceMap.end())
        {
            outObject = getInstanceExport(namedInstanceIt->second,exportName);
            if(outObject)
            {
                if(isA(outObject,type)) { return true; }
                else
                {
                    Log::printf(Log::Category::error,"Resolved import %s.%s to a %s, but was expecting %s",
                                moduleName.c_str(),
                                exportName.c_str(),
                                asString(getObjectType(outObject)).c_str(),
                                asString(type).c_str());
                    return false;
                }
            }
        }
        
        Log::printf(Log::Category::error,"Generated stub for missing import %s.%s : %s\n",moduleName.c_str(),exportName.c_str(),asString(type).c_str());
        outObject = getStubObject(type);
        return true;
    }
    
    Object* getStubObject(ObjectType type) const
    {
        // If the import couldn't be resolved, stub it in.
        switch(type.kind)
        {
            case IR::ObjectKind::function:
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
                stubModule.exports.push_back({"importStub",IR::ObjectKind::function,0});
                stubModuleNames.functions.push_back({"importStub <" + asString(type) + ">",{},{}});
                IR::setDisassemblyNames(stubModule,stubModuleNames);
                IR::validateDefinitions(stubModule);
                
                // Instantiate the module and return the stub function instance.
                auto stubModuleInstance = instantiateModule(compartment,stubModule,{},"importStub");
                return getInstanceExport(stubModuleInstance,"importStub");
            }
            case IR::ObjectKind::memory:
            {
                return asObject(Runtime::createMemory(compartment,asMemoryType(type)));
            }
            case IR::ObjectKind::table:
            {
                return asObject(Runtime::createTable(compartment,asTableType(type)));
            }
            case IR::ObjectKind::global:
            {
                return asObject(Runtime::createGlobal(
                                                      compartment,
                                                      asGlobalType(type),
                                                      IR::Value(asGlobalType(type).valueType,IR::UntaggedValue())));
            }
            case IR::ObjectKind::exceptionType:
            {
                return asObject(Runtime::createExceptionTypeInstance(asExceptionType(type), "importStub"));
            }
            default: Errors::unreachable();
        };
    }
};

// Module imported mathematical functions

// Integer version
DEFINE_INTRINSIC_FUNCTION(env,"_abs",I32,_abs,I32 value) { return std::abs(value); }

//Float (= f32) version
DEFINE_INTRINSIC_FUNCTION(env,"_acosf",F32,_acosf,F32 value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_asinf",F32,_asinf,F32 value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_atanf",F32,_atanf,F32 value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_atan2f",F32,_atan2f,F32 left,F32 right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_cosf",F32,_cosf,F32 value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_expf",F32,_expf,F32 value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_fmodf",F32,_fmodf,F32 left,F32 right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_logf",F32,_logf,F32 value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_log10f",F32,_log10f,F32 value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_powf",F32,_powf,F32 left,F32 right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_remainderf",F32,_remainderf,F32 left,F32 right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_roundf",F32,_roundf,F32 value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_sinf",F32,_sinf,F32 value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_tanf",F32,_tanf,F32 value) { return std::tan(value); }

// Double (= f64) version
DEFINE_INTRINSIC_FUNCTION(env,"_acos",F64,_acos,F64 value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_asin",F64,_asin,F64 value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_atan",F64,_atan,F64 value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_atan2",F64,_atan2,F64 left,F64 right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_cos",F64,_cos,F64 value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_exp",F64,_exp,F64 value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_fmod",F64,_fmod,F64 left,F64 right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_log",F64,_log,F64 value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_log10",F64,_log10,F64 value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_pow",F64,_pow,F64 left,F64 right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_remainder",F64,_remainder,F64 left,F64 right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION(env,"_round",F64,_round,F64 value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_sin",F64,_sin,F64 value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION(env,"_tan",F64,_tan,F64 value) { return std::tan(value); }

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
    compartment = Runtime::createCompartment();
    context = Runtime::createContext(compartment);
    RootResolver rootResolver(compartment);
    
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
    
    fModuleInstance = instantiateModule(compartment, *module, std::move(linkResult.resolvedImports), "Module");
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
    auto functionResult = invokeFunctionChecked(context,fGetNumInputs, invokeArgs);
    return functionResult[0].i32;
}
        
int wasm_dsp::getNumOutputs()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    auto functionResult = invokeFunctionChecked(context,fGetNumOutputs, invokeArgs);
    return functionResult[0].i32;
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
    auto functionResult = invokeFunctionChecked(context,fGetSampleRate, invokeArgs);
    return functionResult[0].i32;
}

void wasm_dsp::init(int samplingRate)
{
    std::cerr << "wasm_dsp::init" << std::endl;
    
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunctionChecked(context,fInit, invokeArgs);
    
    std::cerr << "wasm_dsp::init OK" << std::endl;
}

void wasm_dsp::instanceInit(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunctionChecked(context,fInstanceInit, invokeArgs);
}

void wasm_dsp::instanceConstants(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunctionChecked(context,fInstanceConstants, invokeArgs);
}

void wasm_dsp::instanceResetUserInterface()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    invokeFunctionChecked(context,fInstanceResetUI, invokeArgs);
}

void wasm_dsp::instanceClear()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    invokeArgs.push_back(dsp_arg);
    invokeFunctionChecked(context,fInstanceClear, invokeArgs);
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

