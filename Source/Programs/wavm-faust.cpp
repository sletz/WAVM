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

#define JACK_GTK 1

#include "CLI.h"

#include <map>
#include <unistd.h>
#include <libgen.h>

#include "wasm_dsp_aux.hh"

#ifdef JACK_GTK
    #include "faust/gui/faustgtk.h"
    #include "faust/audio/jack-dsp.h"
#else
    #include "faust/audio/dummy-audio.h"
#endif

using namespace IR;
using namespace Runtime;

#ifdef JACK_GTK
// Faust globals
std::list<GUI*> GUI::fGuiList;
ztimedmap GUI::gTimedZoneMap;
#endif

void showHelp()
{
	std::cerr << "Usage: wavm-faust [switches] [programfile]" << std::endl;
	std::cerr << "  in.wast|in.wasm\t\tSpecify program file (.wast/.wasm)" << std::endl;
	std::cerr << "  -c|--check\t\t\tExit after checking that the program is valid" << std::endl;
	std::cerr << "  -d|--debug\t\t\tWrite additional debug information to stdout" << std::endl;
	std::cerr << "  --\t\t\t\tStop parsing arguments" << std::endl;
}

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

int mainBody(const char* filename,const char* functionName,bool onlyCheck,char** args)
{
	Module module;
	if(filename)
	{
		if(!loadModule(filename,module)) { return EXIT_FAILURE; }
	}
	else
	{
		showHelp();
		return EXIT_FAILURE;
	}

	if(onlyCheck) { return EXIT_SUCCESS; }

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
    
    usleep(2000000);
    
    // Create DSP from the wasm module
    dsp* DSP = new wasm_dsp(moduleInstance);

#ifdef JACK_GTK
    // Create GUI
    GUI* interface = new GTKUI(basename((char*)filename), nullptr, nullptr);
    DSP->buildUserInterface(interface);
    // Create JACK audio driver with the DSP
    jackaudio audio;
#else
    // Create dummy audio driver with the DSP
    dummyaudio audio;
#endif
    
    audio.init(basename((char*)filename), DSP);
    audio.start();
   
#ifdef JACK_GTK
    // Run the GTK GUI
    interface->run();
#endif
  
    audio.stop();
    delete DSP;
    
    return EXIT_SUCCESS;
}

int commandMain(int argc,char** argv)
{
	const char* filename = nullptr;
	const char* functionName = nullptr;

	bool onlyCheck = false;
	auto args = argv;
	while(*++args)
	{
		if(!strcmp(*args, "--function") || !strcmp(*args, "-f"))
		{
			if(!*++args) { showHelp(); return EXIT_FAILURE; }
			functionName = *args;
		}
		else if(!strcmp(*args, "--check") || !strcmp(*args, "-c"))
		{
			onlyCheck = true;
		}
		else if(!strcmp(*args, "--debug") || !strcmp(*args, "-d"))
		{
			Log::setCategoryEnabled(Log::Category::debug,true);
		}
		else if(!strcmp(*args, "--"))
		{
			++args;
			break;
		}
		else if(!strcmp(*args, "--help") || !strcmp(*args, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!filename)
		{
			filename = *args;
		}
		else { break; }
	}

	Runtime::init();

	int returnCode = EXIT_FAILURE;
	#ifdef __AFL_LOOP
	while(__AFL_LOOP(2000))
	#endif
	{
     	returnCode = mainBody(filename,functionName,onlyCheck,args);
        std::cout << "freeUnreferencedObjects " << std::endl;
		Runtime::freeUnreferencedObjects({});
	}
	return returnCode;
}
