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

#ifndef EMCC_DSP_AUX_H
#define EMCC_DSP_AUX_H

#include <string>
#include <vector>

#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"
#include "JSONUIDecoder1.hh"

using namespace Runtime;

/**
 * DSP instance class with methods.
 */
class emcc_dsp : public dsp {
    
    private:
    
        std::string fName;
    
        int fWasmInputs;        // Index in wasm memory
        int fWasmOutputs;       // Index in wasm memory
    
        //FAUSTFLOAT** fInputs;   // Wasm memory mapped to pointers
         //FAUSTFLOAT** fOutputs;  // Wasm memory mapped to pointers
    
        ModuleInstance* fModuleInstance;
    
        // Link the module with the intrinsic modules.
        Compartment* compartment;
        Context* context;
    
        FunctionInstance* fNew;
        FunctionInstance* fDelete;
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
        FunctionInstance* fJSON;
    
        int fDSP;
    
        //JSONUIDecoder2* fDecoder;
    
        // Internal helper methods
    
        FAUSTFLOAT getParamValue(int index)
        {
            std::vector<Value> invokeArgs;
            Value dsp_arg = fDSP;
            Value index_arg = index;
            invokeArgs.push_back(dsp_arg);
            invokeArgs.push_back(index_arg);
            auto functionResult = invokeFunctionChecked(context,fGetParamValue, invokeArgs);
            return (sizeof(FAUSTFLOAT) == 4) ? functionResult.f32 : functionResult.f64;
        }
        
        void setParamValue(int index, FAUSTFLOAT value)
        {
            std::vector<Value> invokeArgs;
            Value dsp_arg = fDSP;
            Value index_arg = index;
            Value value_arg = value;
            invokeArgs.push_back(dsp_arg);
            invokeArgs.push_back(index_arg);
            invokeArgs.push_back(value_arg);
            invokeFunctionChecked(context,fSetParamValue, invokeArgs);
        }
    
        void computeAux(int count)
        {
            // Call wasm compute
            std::vector<Value> invokeArgs;
            Value dsp_arg = fDSP;
            Value count_arg = count;
            Value ins_arg = I32(fWasmInputs);
            Value outs_arg = I32(fWasmOutputs);
            invokeArgs.push_back(dsp_arg);
            invokeArgs.push_back(count_arg);
            invokeArgs.push_back(ins_arg);
            invokeArgs.push_back(outs_arg);
            invokeFunctionChecked(context,fCompute, invokeArgs);
        }
    
        virtual ~emcc_dsp();
    
    public:
    
        static IR::Module* gFactoryModule;
    
        static bool init(const char* filename);
    
        emcc_dsp(IR::Module* module, const string& name);
    
        int getNumInputs();
        
        int getNumOutputs();
        
        void buildUserInterface(UI* ui_interface);
        
        int getSampleRate();
        
        void init(int samplingRate);
        
        void instanceInit(int samplingRate);
    
        void instanceConstants(int samplingRate);
    
        void instanceResetUserInterface();
        
        void instanceClear();
        
        emcc_dsp* clone();
        
        void metadata(Meta* m);
        
        void compute(int count, FAUSTFLOAT** input, FAUSTFLOAT** output);
    
};

inline std::string extractName(const std::string& name)
{
    // determine position of the last '.'
    unsigned int p2 = name.size();
    for (unsigned int i = 0; i < name.size(); i++) {
        if (name[i] == '.')  { p2 = i; }
    }
    return name.substr(0, p2);
}

#endif
