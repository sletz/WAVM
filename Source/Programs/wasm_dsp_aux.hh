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

#ifndef WASM_DSP_AUX_H
#define WASM_DSP_AUX_H

#include <string>
#include <vector>

#ifdef DOUBLE_TEST
#ifndef FAUSTFLOAT
#define FAUSTFLOAT double
#endif

#endif

#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"
#include "JSONUIDecoder1.hh"

using namespace Runtime;

#define DSP_BASE 0  // method 'dsp' parameter is actually 0


/**
 * DSP instance class with methods.
 */
class wasm_dsp : public dsp {
    
    private:
    
        int fWasmInputs;        // Index in wasm memory
        int fWasmOutputs;       // Index in wasm memory
    
        FAUSTFLOAT** fInputs;   // Wasm memory mapped to pointers
        FAUSTFLOAT** fOutputs;  // Wasm memory mapped to pointers
    
        ModuleInstance* fModuleInstance;
    
        FunctionInstance* fGetNumInputs;
        FunctionInstance* fGetNumOutputs;
        FunctionInstance* fInit;
        FunctionInstance* fInstanceInit;
        FunctionInstance* fInstanceConstants;
        FunctionInstance* fInstanceResetUI;
        FunctionInstance* fInstanceClear;
        FunctionInstance* fGetSampleRate;
        FunctionInstance* fSetParamValue;
        FunctionInstance* fGetParamValue;
        FunctionInstance* fCompute;
    
        JSONUIDecoder1* fDecoder;
    
        // Internal helper methods
    
        FAUSTFLOAT getParamValue(int index)
        {
            std::vector<Value> invokeArgs;
            Value dsp_arg = DSP_BASE;
            Value index_arg = index;
            invokeArgs.push_back(dsp_arg);
            invokeArgs.push_back(index_arg);
            auto functionResult = invokeFunction(fGetParamValue, invokeArgs);
            return (sizeof(FAUSTFLOAT) == 4) ? functionResult.f32 : functionResult.f64;
        }
        
        void setParamValue(int index, FAUSTFLOAT value)
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
    
        void computeAux(int count)
        {
            // Call wasm compute
            std::vector<Value> invokeArgs;
            Value dsp_arg = DSP_BASE;
            Value count_arg = count;
            Value ins_arg = I32(fWasmInputs);
            Value outs_arg = I32(fWasmOutputs);
            invokeArgs.push_back(dsp_arg);
            invokeArgs.push_back(count_arg);
            invokeArgs.push_back(ins_arg);
            invokeArgs.push_back(outs_arg);
            invokeFunction(fCompute, invokeArgs);
        }
    
        std::string getJSON(U8* base_ptr)
        {
            int i = 0;
            std::string json = "";
            while (base_ptr[i] != 0) {
                json += base_ptr[i++];
            }
            return json;
        }
 
        virtual ~wasm_dsp();
    
    public:
    
        static IR::Module* gFactoryModule;
    
        static bool init(const char* filename);
    
        wasm_dsp(IR::Module* module);
    
        int getNumInputs();
        
        int getNumOutputs();
        
        void buildUserInterface(UI* ui_interface);
        
        int getSampleRate();
        
        void init(int samplingRate);
        
        void instanceInit(int samplingRate);
    
        void instanceConstants(int samplingRate);
    
        void instanceResetUserInterface();
        
        void instanceClear();
        
        wasm_dsp* clone();
        
        void metadata(Meta* m);
        
        void compute(int count, FAUSTFLOAT** input, FAUSTFLOAT** output);
    
};

#endif
