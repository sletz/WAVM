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

#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"
#include "faust/gui/SimpleParser.h"

using namespace Runtime;

#define DSP_BASE 0  // method 'dsp' parameter is actually 0

#ifdef _WIN32
#include <windows.h>
#define snprintf _snprintf
#endif

inline FAUSTFLOAT STR2REAL1(const std::string& s) { return (strtod(s.c_str(), NULL)); }

// A JSON decoder that directly maps zone indexes to their effective address in wasm memory
struct JSONUIDecoder1 {
    
    std::string fName;
    
    std::map<std::string, std::string> fMetadatas;
    std::vector<itemInfo*> fUiItems;
    
    std::vector<FAUSTFLOAT*> fInControl;
    std::vector<FAUSTFLOAT*> fOutControl;
    
    std::string fJSON;
    
    int fNumInputs, fNumOutputs;
    int fInputItems, fOutputItems;
    
    std::string fVersion;
    std::string fOptions;
    
    int fDSPSize;
    
    bool isInput(const string& type) { return (type == "vslider" || type == "hslider" || type == "nentry" || type == "button" || type == "checkbox"); }
    bool isOutput(const string& type) { return (type == "hbargraph" || type == "vbargraph"); }
    
    JSONUIDecoder1(const std::string& json, MemoryInstance* memory)
    {
        fJSON = json;
        const char* p = fJSON.c_str();
        parseJson(p, fMetadatas, fUiItems);
        
        // fMetadatas will contain the "meta" section as well as <name : val>, <inputs : val>, <ouputs : val> pairs
        if (fMetadatas.find("name") != fMetadatas.end()) {
            fName = fMetadatas["name"];
            fMetadatas.erase("name");
        } else {
            fName = "";
        }
        
        if (fMetadatas.find("version") != fMetadatas.end()) {
            fVersion = fMetadatas["version"];
            fMetadatas.erase("version");
        } else {
            fVersion = "";
        }
        
        if (fMetadatas.find("options") != fMetadatas.end()) {
            fOptions = fMetadatas["options"];
            fMetadatas.erase("options");
        } else {
            fOptions = "";
        }
        
        if (fMetadatas.find("size") != fMetadatas.end()) {
            fDSPSize = atoi(fMetadatas["size"].c_str());
            fMetadatas.erase("size");
        } else {
            fDSPSize = -1;
        }
        
        if (fMetadatas.find("inputs") != fMetadatas.end()) {
            fNumInputs = atoi(fMetadatas["inputs"].c_str());
            fMetadatas.erase("inputs");
        } else {
            fNumInputs = -1;
        }
        
        if (fMetadatas.find("outputs") != fMetadatas.end()) {
            fNumOutputs = atoi(fMetadatas["outputs"].c_str());
            fMetadatas.erase("outputs");
        } else {
            fNumOutputs = -1;
        }
        
        fInputItems = 0;
        fOutputItems = 0;
        
        vector<itemInfo*>::iterator it;
        for (it = fUiItems.begin(); it != fUiItems.end(); it++) {
            string type = (*it)->type;
            if (isInput(type)) {
                fInputItems++;
            } else if (isOutput(type)) {
                fOutputItems++;
            }
        }
        
        int counterIn = 0;
        int counterOut = 0;
        
        // Prepare the fPathTable and init zone
        for (it = fUiItems.begin(); it != fUiItems.end(); it++) {
            string type = (*it)->type;
            // Meta data declaration for input items
            if (isInput(type)) {
                if ((*it)->address != "") {
                    int index = atoi((*it)->index.c_str());
                    FAUSTFLOAT* zone = reinterpret_cast<FAUSTFLOAT*>(getValidatedMemoryOffsetRange(memory, index, 0));
                    fInControl.push_back(zone);
                }
                counterIn++;
            }
            // Meta data declaration for output items
            else if (isOutput(type)) {
                if ((*it)->address != "") {
                    int index = atoi((*it)->index.c_str());
                    FAUSTFLOAT* zone = reinterpret_cast<FAUSTFLOAT*>(getValidatedMemoryOffsetRange(memory, index, 0));
                    fOutControl.push_back(zone);
                }
                counterOut++;
            }
        }
    }
    
    virtual ~JSONUIDecoder1()
    {
        vector<itemInfo*>::iterator it;
        for (it = fUiItems.begin(); it != fUiItems.end(); it++) {
            delete(*it);
        }
    }
    
    void metadata(Meta* m)
    {
        std::map<std::string, std::string>::iterator it;
        for (it = fMetadatas.begin(); it != fMetadatas.end(); it++) {
            m->declare((*it).first.c_str(), (*it).second.c_str());
        }
    }
    
    void resetUserInterface()
    {
        vector<itemInfo*>::iterator it;
        int item = 0;
        for (it = fUiItems.begin(); it != fUiItems.end(); it++) {
            if (isInput((*it)->type)) {
                *fInControl[item++] = STR2REAL1((*it)->init);
            }
        }
    }
    
    void buildUserInterface(UI* ui)
    {
        // To be sure the floats are correctly encoded
        char* tmp_local = setlocale(LC_ALL, NULL);
        setlocale(LC_ALL, "C");
        
        int counterIn = 0;
        int counterOut = 0;
        vector<itemInfo*>::iterator it;
        
        for (it = fUiItems.begin(); it != fUiItems.end(); it++) {
            
            bool isInItem = false;
            bool isOutItem = false;
            string type = (*it)->type;
            
            FAUSTFLOAT init = STR2REAL1((*it)->init);
            FAUSTFLOAT min = STR2REAL1((*it)->min);
            FAUSTFLOAT max = STR2REAL1((*it)->max);
            FAUSTFLOAT step = STR2REAL1((*it)->step);
            
            if (isInput(type)) {
                isInItem = true;
            } else if (isOutput(type)) {
                isOutItem = true;
            }
            
            // Meta data declaration for input items
            if (isInput(type)) {
                *fInControl[counterIn] = init;
                for (int i = 0; i < (*it)->meta.size(); i++) {
                    ui->declare(fInControl[counterIn], (*it)->meta[i].first.c_str(), (*it)->meta[i].second.c_str());
                }
            }
            // Meta data declaration for output items
            else if (isOutput(type)) {
                *fOutControl[counterOut] = init;
                for (int i = 0; i < (*it)->meta.size(); i++) {
                    ui->declare(fOutControl[counterOut], (*it)->meta[i].first.c_str(), (*it)->meta[i].second.c_str());
                }
            }
            // Meta data declaration for group opening or closing
            else {
                for (int i = 0; i < (*it)->meta.size(); i++) {
                    ui->declare(0, (*it)->meta[i].first.c_str(), (*it)->meta[i].second.c_str());
                }
            }
            
            if (type == "hgroup") {
                ui->openHorizontalBox((*it)->label.c_str());
            } else if (type == "vgroup") {
                ui->openVerticalBox((*it)->label.c_str());
            } else if (type == "tgroup") {
                ui->openTabBox((*it)->label.c_str());
            } else if (type == "vslider") {
                ui->addVerticalSlider((*it)->label.c_str(), fInControl[counterIn], init, min, max, step);
            } else if (type == "hslider") {
                ui->addHorizontalSlider((*it)->label.c_str(), fInControl[counterIn], init, min, max, step);
            } else if (type == "checkbox") {
                ui->addCheckButton((*it)->label.c_str(), fInControl[counterIn]);
            } else if (type == "hbargraph") {
                ui->addHorizontalBargraph((*it)->label.c_str(), fOutControl[counterOut], min, max);
            } else if (type == "vbargraph") {
                ui->addVerticalBargraph((*it)->label.c_str(), fOutControl[counterOut], min, max);
            } else if (type == "nentry") {
                ui->addNumEntry((*it)->label.c_str(), fInControl[counterIn], init, min, max, step);
            } else if (type == "button") {
                ui->addButton((*it)->label.c_str(), fInControl[counterIn]);
            } else if (type == "close") {
                ui->closeBox();
            }
            
            if (isInItem) {
                counterIn++;
            }
            
            if (isOutItem) {
                counterOut++;
            }
        }
        
        setlocale(LC_ALL, tmp_local);
    }
    
};

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
