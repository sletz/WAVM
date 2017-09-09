#include "Inline/BasicTypes.h"
#include "Inline/Timing.h"
#include "Platform/Platform.h"
#include "WAST/WAST.h"
#include "Runtime/Runtime.h"
#include "Runtime/Linker.h"
#include "Runtime/Intrinsics.h"
#include "Emscripten/Emscripten.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Validate.h"

#include "CLI.h"

#include <map>
#include <unistd.h>
#include <libgen.h>

#include "wasm_dsp_aux.hh"

#include "faust/dsp/poly-dsp.h"
#include "faust/gui/FUI.h"
#include "faust/gui/MidiUI.h"
#include "faust/gui/httpdUI.h"
#include "faust/gui/OSCUI.h"

#include "faust/gui/faustgtk.h"
#include "faust/audio/jack-dsp.h"

#include "faust/misc.h"

using namespace IR;
using namespace Runtime;

// Faust globals
std::list<GUI*> GUI::fGuiList;
ztimedmap GUI::gTimedZoneMap;

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

int mainBody(const char* filename_aux, int argc, char** args)
{
    if (isopt(args, "-h") || isopt(args, "-help")) {
        cout << "faust-wavm [-nvoices N] [-midi] [-osc] [-httpd] foo.wasm" << endl;
        exit(EXIT_FAILURE);
    }
    
    Module module;
    if(filename_aux)
    {
        if(!loadModule(filename_aux,module)) { return EXIT_FAILURE; }
    }
    else
    {
        return EXIT_FAILURE;
    }

    // Link and instantiate the module.
    RootResolver rootResolver;
    LinkResult linkResult = linkModule(module,rootResolver);
    if(!linkResult.success)
    {
        std::cerr << "Failed to link module:" << std::endl;
        for(auto& missingImport : linkResult.missingImports)
        {
            std::cerr << "Missing import: module=\"" << missingImport.moduleName
                << "\" export=\"" << missingImport.exportName
                << "\" type=\"" << asString(missingImport.type) << "\"" << std::endl;
        }
        return EXIT_FAILURE;
    }
    ModuleInstance* moduleInstance = instantiateModule(module,std::move(linkResult.resolvedImports));
    if(!moduleInstance) { return EXIT_FAILURE; }
    Emscripten::initInstance(module,moduleInstance);
    
    char name[256];
    char filename[256];
    char rcfilename[256];
    char* home = getenv("HOME");
    
    snprintf(name, 255, "%s", basename(args[0]));
    snprintf(filename, 255, "%s", basename(args[argc-1]));
    snprintf(rcfilename, 255, "%s/.%s-%src", home, name, filename);
    
    bool is_midi = isopt(args, "-midi");
    bool is_osc = isopt(args, "-osc");
    bool is_httpd = isopt(args, "-httpd");
    int nvoices = lopt(args, "-nvoices", -1);
    
    // Create DSP from the wasm module
    mydsp_poly* dsp_poly = nullptr;
    MidiUI* midiinterface = nullptr;
    httpdUI* httpdinterface = nullptr;
    GUI* oscinterface = nullptr;
    
    dsp* DSP = new wasm_dsp(moduleInstance);
    
    if (nvoices > 0) {
        cout << "Starting polyphonic mode nvoices : " << nvoices << endl;
        DSP = dsp_poly = new mydsp_poly(DSP, nvoices, true, false);
    }

    // Create GUI
    GUI* interface = new GTKUI(basename((char*)filename), nullptr, nullptr);
    DSP->buildUserInterface(interface);
    
    FUI* finterface = new FUI();
    DSP->buildUserInterface(finterface);
    finterface->recallState(rcfilename);
    
    // Create JACK audio driver with the DSP
    jackaudio_midi audio;
    
    if (!audio.init(basename((char*)filename), DSP, true)) {
        return 0;
    }
    
    if (is_httpd) {
        httpdinterface = new httpdUI(name, DSP->getNumInputs(), DSP->getNumOutputs(), argc, args);
        DSP->buildUserInterface(httpdinterface);
    }
    
    if (is_osc) {
        oscinterface = new OSCUI(filename, argc, args);
        DSP->buildUserInterface(oscinterface);
    }
    
    if (is_midi) {
        midiinterface = new MidiUI(&audio);
        DSP->buildUserInterface(midiinterface);
    }
    
    if (nvoices > 0) {
        audio.addMidiIn(dsp_poly);
    }

    audio.start();
    
    if (is_httpd) {
        httpdinterface->run();
    }
    
    if (is_osc) {
        oscinterface->run();
    }
    
    if (is_osc) {
        midiinterface->run();
    }
   
    interface->run();
  
    audio.stop();
    
    delete DSP;
    delete interface;
    delete finterface;
    delete midiinterface;
    delete httpdinterface;
    delete oscinterface;
    
    return EXIT_SUCCESS;
}

int commandMain(int argc,char** argv)
{
    const char* filename = argv[argc-1];
  
    Runtime::init();

    int returnCode = EXIT_FAILURE;
    #ifdef __AFL_LOOP
    while(__AFL_LOOP(2000))
    #endif
    {
        returnCode = mainBody(filename, argc, argv);
        std::cout << "freeUnreferencedObjects " << std::endl;
        Runtime::freeUnreferencedObjects({});
    }
    return returnCode;
}
