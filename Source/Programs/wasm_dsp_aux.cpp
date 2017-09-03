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

#include "wasm_dsp_aux.hh"
#include "faust/gui/JSONUIDecoder.h"

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
    
    std::cout << "getMemoryNumPages " << (int)getMemoryNumPages(memory) << std::endl;
    std::cout << "getMemoryMaxPages " << (int)getMemoryMaxPages(memory) << std::endl;
    
    // JSON is located at offset 0 in the memory segment
    fDecoder = new JSONUIDecoder(getJSON(getMemoryBaseAddress(memory)));
    
    int ptr_size = 4;
    int sample_size = sizeof(FAUSTFLOAT);
    int buffer_size = 2048;
    
    numIn = getNumInputs();
    numOut = getNumOutputs();
    
    std::cout << "numIn " << numIn << std::endl;
    std::cout << "numOut " << numOut << std::endl;
    
    // DSP is placed first with index 0. Audio buffer start at the end of DSP.
    int audio_heap_ptr = fDecoder->fDSPSize;
    
    // Setup pointers offset
    int audio_heap_ptr_inputs = audio_heap_ptr;
    int audio_heap_ptr_outputs = audio_heap_ptr_inputs + (numIn * ptr_size);
    
    // Setup buffer offset
    int audio_heap_inputs = audio_heap_ptr_outputs + (numOut * ptr_size);
    int audio_heap_outputs = audio_heap_inputs + (numIn * buffer_size * sample_size);
    
    std::cout << "audio_heap_ptr " << audio_heap_ptr << std::endl;
    std::cout << "audio_heap_ptr_inputs " << audio_heap_ptr_inputs << std::endl;
    std::cout << "audio_heap_ptr_outputs " << audio_heap_ptr_outputs << std::endl;
    std::cout << "audio_heap_inputs " << audio_heap_inputs << std::endl;
    std::cout << "audio_heap_outputs " << audio_heap_outputs << std::endl;
    
    if (numIn > 0) {
        ins = audio_heap_ptr_inputs;
        std::cout << "ins " << ins << std::endl;
        int* HEAP32 = reinterpret_cast<int*>(getValidatedMemoryOffsetRange(memory, ins, 0));
        std::cout << "HEAP32 Ins " << HEAP32 << std::endl;
        
        for (int i = 0; i < numIn; i++) {
            HEAP32[i] = audio_heap_inputs + (buffer_size * sample_size * i);
            std::cout << "HEAP32 input[i]" << " " << i << " " << HEAP32[i] << std::endl;
        }
        
        // Prepare Ins buffer tables
        FAUSTFLOAT* HEAPF32 = reinterpret_cast<FAUSTFLOAT*>(getValidatedMemoryOffsetRange(memory, audio_heap_inputs, 0));
        
        for (int i = 0; i < numIn; i++) {
            dspInChannnels[i] = HEAPF32 + (buffer_size * i);
            std::cout << "dspInChannnels[i] " << dspInChannnels[i] << std::endl;
        }
    }
    
    if (numOut > 0) {
        outs = audio_heap_ptr_outputs;
        std::cout << "outs " << outs << std::endl;
        int* HEAP32 = reinterpret_cast<int*>(getValidatedMemoryOffsetRange(memory, outs, 0));
        std::cout << "HEAP32 outs " << HEAP32 << std::endl;
        
        for (int i = 0; i < numOut; i++) {
            HEAP32[i] = audio_heap_outputs + (buffer_size * sample_size * i);
            std::cout << "HEAP32 output[i]" << " " << i << " " << HEAP32[i] << std::endl;
        }
        
        // Prepare Out buffer tables
        FAUSTFLOAT* HEAPF32 = reinterpret_cast<FAUSTFLOAT*>(getValidatedMemoryOffsetRange(memory, audio_heap_outputs, 0));
        
        for (int i = 0; i < numOut; i++) {
            dspOutChannnels[i] =  HEAPF32 + (buffer_size * i);
            std::cout << "dspOutChannnels[i] " << dspOutChannnels[i] << std::endl;
        }
    }
}

wasm_dsp::~wasm_dsp()
{
    delete fDecoder;
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

// No implemented
void wasm_dsp::metadata(Meta* m) {}

void wasm_dsp::compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs)
{
    try
    {
        // Copy audio inputs
        for (int i = 0; i < numIn; i++) {
            memcpy(dspInChannnels[i], inputs[i], sizeof(FAUSTFLOAT) * count);
        }
       
        // Copy zone value to wasm module input control
        controlMap& input_controls = fDecoder->fPathInputTable;
        controlMap::iterator it;
        for (it = input_controls.begin(); it != input_controls.end(); it++) {
            pair <int, FAUSTFLOAT*> tmp = (*it).second;
            //std::cout << tmp.first << " " << *tmp.second << std::endl;
            setParamValue(tmp.first, *tmp.second);
        }
        
        // Call wasm compute
        std::vector<Value> invokeArgs;
        Value dsp_arg = DSP_BASE;
        Value count_arg = count;
        Value ins_arg = I32(ins);
        Value outs_arg = I32(outs);
        invokeArgs.push_back(dsp_arg);
        invokeArgs.push_back(count_arg);
        invokeArgs.push_back(ins_arg);
        invokeArgs.push_back(outs_arg);
        invokeFunction(fCompute, invokeArgs);
        
        // Copy wasm module output control to zone value
        controlMap& output_controls = fDecoder->fPathOutputTable;
        for (it = output_controls.begin(); it != output_controls.end(); it++) {
            pair <int, FAUSTFLOAT*> tmp = (*it).second;
            *tmp.second = getParamValue(tmp.first);
        }
        
        // Copy audio outputs
        for (int i = 0; i < numOut; i++) {
            memcpy(outputs[i], dspOutChannnels[i], sizeof(FAUSTFLOAT) * count);
        }
        
    } catch(Runtime::Exception exception) {
        std::cerr << "Runtime exception: " << describeExceptionCause(exception.cause) << std::endl;
        for (auto calledFunction : exception.callStack) { std::cerr << "  " << calledFunction << std::endl; }
    }
}

FAUSTFLOAT wasm_dsp::getParamValue(int index)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value index_arg = index;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(index_arg);
    auto functionResult = invokeFunction(fGetParamValue, invokeArgs);
    return (sizeof(FAUSTFLOAT) == 4) ? functionResult.f32 : functionResult.f64;
}

void wasm_dsp::setParamValue(int index, FAUSTFLOAT value)
{
    std::vector<Value> invokeArgs;
    Value dsp_arg = DSP_BASE;
    Value index_arg = index;
    Value value_arg = value;
    invokeArgs.push_back(dsp_arg);
    invokeArgs.push_back(index_arg);
    invokeArgs.push_back(value_arg);
    invokeFunction(fSetParamValue, invokeArgs);
}

std::string wasm_dsp::getJSON(U8* base_ptr)
{
    int i = 0;
    std::string json = "";
    while (base_ptr[i] != 0) {
        json += base_ptr[i++];
    }
    return json;
}


