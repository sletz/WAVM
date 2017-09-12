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

#include "Runtime/Intrinsics.h"
#include "Emscripten/Emscripten.h"
#include "IR/Module.h"
#include "Runtime/Linker.h"
#include "CLI.h"

#include "emcc_dsp_aux.hh"

using namespace IR;
using namespace Runtime;

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

Module* emcc_dsp::gFactoryModule = nullptr;

bool emcc_dsp::init(const char* filename)
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

emcc_dsp::emcc_dsp(Module* module, const string& name)
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
    Emscripten::initInstance(*module,fModuleInstance);
    
    if (!fModuleInstance) {
        std::cerr << "Failed to instantiateModule" << std::endl;
        throw std::bad_alloc();
    }
    
    fName = name;
    
    fNew = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name + "_constructor"));
    if (!fNew) {
        std::cerr << "Error : module is not compiled with Emscripten...\n";
        throw std::bad_alloc();
    }
    fDelete = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_destructor"));
    fGetNumInputs = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_getNumInputs"));
    fGetNumOutputs = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_getNumOutputs"));
    fGetSampleRate = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_getSampleRate"));
    fInit = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_init"));
    fInstanceInit = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_instanceInit"));
    fInstanceConstants = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_instanceConstants"));
    fInstanceResetUI = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_instanceResetUserInterface"));
    fInstanceClear = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_instanceClear"));
    fSetParamValue = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_setParamValue"));
    fGetParamValue = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_getParamValue"));
    fCompute = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_compute"));
    fJSON = asFunctionNullable(getInstanceExport(fModuleInstance, "_" + name +  "_getJSON"));
    
    std::cerr << "instantiateModule" << std::endl;
    
    std::vector<Value> invokeArgs;
    auto functionResult = invokeFunction(fNew, invokeArgs);
    fDSP = functionResult.i32;
    
    std::cerr << "fDSP " << fDSP << std::endl;
    
    /*
    MemoryInstance* memory = getDefaultMemory(fModuleInstance);
    
    // JSON is located at offset 0 in the memory segment
    fDecoder = new JSONUIDecoder1(getJSON(getMemoryBaseAddress(memory)), memory);
    
    int ptr_size = 4;
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
    */
}

emcc_dsp::~emcc_dsp()
{
    /*
    delete fDecoder;
    delete [] fInputs;
    delete [] fOutputs;
    */
    
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    invokeFunction(fDelete, invokeArgs);
}

int emcc_dsp::getNumInputs()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    invokeArgs.push_back(dsp_arg);
    auto functionResult = invokeFunction(fGetNumInputs, invokeArgs);
    return functionResult.i32;
}
        
int emcc_dsp::getNumOutputs()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    invokeArgs.push_back(dsp_arg);
    auto functionResult = invokeFunction(fGetNumOutputs, invokeArgs);
    return functionResult.i32;
}

void emcc_dsp::buildUserInterface(UI* ui_interface)
{
    //fDecoder->buildUserInterface(ui_interface);
}

int emcc_dsp::getSampleRate()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    invokeArgs.push_back(dsp_arg);
    auto functionResult = invokeFunction(fGetSampleRate, invokeArgs);
    return functionResult.i32;
}

void emcc_dsp::init(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunction(fInit, invokeArgs);
}

void emcc_dsp::instanceInit(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunction(fInstanceInit, invokeArgs);
}

void emcc_dsp::instanceConstants(int samplingRate)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    Value samplingRate_arg = samplingRate;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(samplingRate_arg);
    invokeFunction(fInstanceConstants, invokeArgs);
}

void emcc_dsp::instanceResetUserInterface()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    invokeArgs.push_back(dsp_arg);
    invokeFunction(fInstanceResetUI, invokeArgs);
}

void emcc_dsp::instanceClear()
{
    std::vector<Value> invokeArgs;
    Value dsp_arg(fDSP);
    invokeArgs.push_back(dsp_arg);
    invokeFunction(fInstanceClear, invokeArgs);
}

emcc_dsp* emcc_dsp::clone()
{
    return new emcc_dsp(emcc_dsp::gFactoryModule, fName);
}

void emcc_dsp::metadata(Meta* m)
{
    //fDecoder->metadata(m);
}

void emcc_dsp::compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs)
{
    /*
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
        std::cerr << "Runtime exception: " << describeExceptionCause(exception.cause) << std::endl;
        for (auto calledFunction : exception.callStack) { std::cerr << "  " << calledFunction << std::endl; }
    }
    */
}

