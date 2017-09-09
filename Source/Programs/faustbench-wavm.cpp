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

#include <map>
#include <unistd.h>
#include <libgen.h>

#include "wasm_dsp_aux.hh"
#include "faust/dsp/dsp-bench.h"
#include "faust/misc.h"

using namespace IR;
using namespace Runtime;

int mainBody(const char* filename_aux, int argc, char** args)
{
    if (isopt(args, "-h") || isopt(args, "-help")) {
        std::cout << "faustbench-wavm foo.wasm" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Load and init the module
    if (!wasm_dsp::init(filename_aux)) {
        return EXIT_FAILURE;
    }
    
    // Create an instance
    dsp* DSP = new wasm_dsp(wasm_dsp::gFactoryModule);
    
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

int main(int argc,char** argv)
{
    try
    {
        return commandMain(argc,argv);
    }
    catch(IR::ValidationException exception)
    {
        std::cerr << "Failed to validate module: " << exception.message << std::endl;
        return EXIT_FAILURE;
    }
    catch(Runtime::Exception exception)
    {
        std::cerr << "Runtime exception: " << describeExceptionCause(exception.cause) << std::endl;
        for(auto calledFunction : exception.callStack) { std::cerr << "  " << calledFunction << std::endl; }
        return EXIT_FAILURE;
    }
    catch(Serialization::FatalSerializationException exception)
    {
        std::cerr << "Fatal serialization exception: " << exception.message << std::endl;
        return EXIT_FAILURE;
    }
}
