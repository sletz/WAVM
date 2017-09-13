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
#include "emcc_dsp_aux.hh"

#include "faust/dsp/dsp-bench.h"
#include "faust/misc.h"

using namespace IR;
using namespace Runtime;

int mainBody(const char* filename, int argc, char** args)
{
    if (isopt(args, "-h") || isopt(args, "-help")) {
        std::cout << "faustbench-wavm [-emcc] [-run <num>] foo.wasm" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    int run = lopt(args, "-run", 1);
    bool is_emcc = isopt(args, "-emcc");
    
    //FAUSTBENCH_LOG<string>("faustbench WAVM");

    // Load and init the module
    if (is_emcc) {
        // Load and init the emcc module
        if (!emcc_dsp::init(filename)) {
            return EXIT_FAILURE;
        }
    } else {
        // Load and init the wasm module
        if (!wasm_dsp::init(filename)) {
            return EXIT_FAILURE;
        }
    }
    
    // Create an instance
    dsp* DSP = nullptr;
    
    try {
        if (is_emcc) {
            DSP = new emcc_dsp(emcc_dsp::gFactoryModule, extractName(filename));
        } else {
            DSP = new wasm_dsp(wasm_dsp::gFactoryModule);
        }
    } catch (...) {
        std::cerr << "Error : cannot allocate DSP\n";
        return EXIT_FAILURE;
    }
    
    measure_dsp mes(DSP, 1024, 5.);  // Buffer_size and duration in sec of measure
    for (int i = 0; i < run; i++) {
        mes.measure();
        std::cout << args[0] << " : " << mes.getStats() << " " << "(DSP CPU % : " << (mes.getCPULoad() * 100) << ")" << std::endl;
        FAUSTBENCH_LOG<double>(mes.getStats());
    }
    
    // DSP is deallocated by measure_dsp
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
