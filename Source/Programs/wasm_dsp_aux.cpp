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

#include "wasm_dsp_aux.hh"
#include "faust/gui/JSONUIDecoder.h"
#include "Runtime/Intrinsics.h"

// Module imported mathematical functions

// Integer version
DEFINE_INTRINSIC_FUNCTION1(global.Math,abs,abs,i32,i32,value) { return std::abs(value); }

// Float (= f32) version
DEFINE_INTRINSIC_FUNCTION1(global.Math,acos,acos,f32,f32,value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,asin,asin,f32,f32,value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,atan,atan,f32,f32,value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION2(global.Math,atan2,atan2,f32,f32,left,f32,right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,cos,cos,f32,f32,value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,exp,exp,f32,f32,value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION2(asm2wasm,fmod,fmod,f32,f32,left,f32,right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,log,log,f32,f32,value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION1(asm2wasm,log10,log10,f32,f32,value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION2(global.Math,pow,pow,f32,f32,left,f32,right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION2(asm2wasm,remainder,remainder,f32,f32,left,f32,right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,round,round,f32,f32,value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,sin,sin,f32,f32,value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,tan,tan,f32,f32,value) { return std::tan(value); }

// Double (= f64) version
DEFINE_INTRINSIC_FUNCTION1(global.Math,acos,acos,f64,f64,value) { return std::acos(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,asin,asin,f64,f64,value) { return std::asin(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,atan,atan,f64,f64,value) { return std::atan(value); }
DEFINE_INTRINSIC_FUNCTION2(global.Math,atan2,atan2,f64,f64,left,f64,right) { return std::atan2(left,right); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,cos,cos,f64,f64,value) { return std::cos(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,exp,exp,f64,f64,value) { return std::exp(value); }
DEFINE_INTRINSIC_FUNCTION2(asm2wasm,fmod,fmod,f64,f64,left,f64,right) { return std::fmod(left,right); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,log,log,f64,f64,value) { return std::log(value); }
DEFINE_INTRINSIC_FUNCTION1(asm2wasm,log10,log10,f64,f64,value) { return std::log10(value); }
DEFINE_INTRINSIC_FUNCTION2(global.Math,pow,pow,f64,f64,left,f64,right) { return std::pow(left,right); }
DEFINE_INTRINSIC_FUNCTION2(asm2wasm,remainder,remainder,f64,f64,left,f64,right) { return std::remainder(left,right); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,round,round,f64,f64,value) { return std::round(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,sin,sin,f64,f64,value) { return std::sin(value); }
DEFINE_INTRINSIC_FUNCTION1(global.Math,tan,tan,f64,f64,value) { return std::tan(value); }

// Public DSP API

wasm_dsp::wasm_dsp(ModuleInstance* instance)
{
    fModuleInstance = instance;
    
    fGetNumInputs = asFunctionNullable(getInstanceExport(instance, "getNumInputs"));
    fGetNumOutputs = asFunctionNullable(getInstanceExport(instance, "getNumOutputs"));
    fGetSampleRate = asFunctionNullable(getInstanceExport(instance, "getSampleRate"));
    fInit = asFunctionNullable(getInstanceExport(instance, "init"));
    fInstanceInit = asFunctionNullable(getInstanceExport(instance, "instanceInit"));
    fInstanceConstants = asFunctionNullable(getInstanceExport(instance, "instanceConstants"));
    fInstanceResetUI = asFunctionNullable(getInstanceExport(instance, "instanceResetUserInterface"));
    fInstanceClear = asFunctionNullable(getInstanceExport(instance, "instanceClear"));
    fSetParamValue = asFunctionNullable(getInstanceExport(instance, "setParamValue"));
    fGetParamValue = asFunctionNullable(getInstanceExport(instance, "getParamValue"));
    fCompute = asFunctionNullable(getInstanceExport(instance, "compute"));
    
    MemoryInstance* memory = getDefaultMemory(instance);
    
    // JSON is located at offset 0 in the memory segment
    fDecoder = new JSONUIDecoder(getJSON(getMemoryBaseAddress(memory)));
    
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
    return new wasm_dsp(fModuleInstance);
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
       
        // Copy zone value to wasm module input control
        controlMap& input_controls = fDecoder->fPathInputTable;
        controlMap::iterator it;
        for (it = input_controls.begin(); it != input_controls.end(); it++) {
            pair <int, FAUSTFLOAT*> tmp = (*it).second;
            setParamValue(tmp.first, *tmp.second);
        }
        
        // Call wasm compute
        computeAux(count);
        
        // Copy wasm module output control to zone value
        controlMap& output_controls = fDecoder->fPathOutputTable;
        for (it = output_controls.begin(); it != output_controls.end(); it++) {
            pair <int, FAUSTFLOAT*> tmp = (*it).second;
            *tmp.second = getParamValue(tmp.first);
        }
        
        // Copy audio outputs
        for (int i = 0; i < fDecoder->fNumOutputs; i++) {
            memcpy(outputs[i], fOutputs[i], sizeof(FAUSTFLOAT) * count);
        }
        
    } catch(Runtime::Exception exception) {
        std::cerr << "Runtime exception: " << describeExceptionCause(exception.cause) << std::endl;
        for (auto calledFunction : exception.callStack) { std::cerr << "  " << calledFunction << std::endl; }
    }
}

