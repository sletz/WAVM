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
#include "faust/dsp/dsp-bench.h"
#include "faust/misc.h"

using namespace IR;
using namespace Runtime;

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
        std::cout << "faustbench-wavm foo.wasm" << std::endl;
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
    
    dsp* DSP = new wasm_dsp(moduleInstance);
    
    DSP->init(48000);
    measure_dsp mes(DSP, 1024, 5);
    for (int i = 0; i < 5; i++) {
        mes.measure();
        std::cout << args[0] << " : " << mes.getStats() << " " << "(DSP CPU % : " << (mes.getCPULoad() * 100) << ")" << std::endl;
    }
    
    delete DSP;
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
