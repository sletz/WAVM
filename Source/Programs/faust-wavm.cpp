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

int mainBody(const char* filename_aux, int argc, char** args)
{
    if (isopt(args, "-h") || isopt(args, "-help")) {
        cout << "faust-wavm [-emcc] [-nvoices N] [-midi] [-osc] [-httpd] foo.wasm" << endl;
        exit(EXIT_FAILURE);
    }
    
    bool is_emcc = isopt(args, "-emcc");
    
    // Load and init the module
    if (is_emcc) {
        // Load and init the emcc module
        if (!emcc_dsp::init(filename_aux)) {
            return EXIT_FAILURE;
        }
    } else {
        // Load and init the wasm module
        if (!wasm_dsp::init(filename_aux)) {
            return EXIT_FAILURE;
        }
    }
    
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
    
    if (nvoices > 0) {
        cout << "Starting polyphonic mode nvoices : " << nvoices << endl;
        DSP = dsp_poly = new mydsp_poly(DSP, nvoices, false, false);
    }

    // Create GUI
    GUI* interface = new GTKUI(basename((char*)filename), nullptr, nullptr);
    DSP->buildUserInterface(interface);
    
    FUI* finterface = new FUI();
    DSP->buildUserInterface(finterface);
    
    // Create JACK audio driver with the DSP
    jackaudio_midi audio;
    
    if (!audio.init(basename((char*)filename), DSP)) {
        return 0;
    }
    
    // After audio.init that calls 'init'
    finterface->recallState(rcfilename);
    
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
    
    finterface->saveState(rcfilename);
    
    delete DSP;
    delete interface;
    delete finterface;
    delete midiinterface;
    delete httpdinterface;
    delete oscinterface;
    
    return EXIT_SUCCESS;
}

int commandMain(int argc, char** argv)
{
    const char* filename = argv[argc-1];
  
    // Steph : 19/04/18
    //Runtime::init();

    int returnCode = EXIT_FAILURE;
    #ifdef __AFL_LOOP
    while(__AFL_LOOP(2000))
    #endif
    {
        returnCode = mainBody(filename, argc, argv);
        //std::cout << "freeUnreferencedObjects " << std::endl;
        //Runtime::freeUnreferencedObjects({});
    }
    return returnCode;
}

int main(int argc,char** argv)
{
    if(argc != 2) {
        std::cerr <<  "Usage: Test in.wast" << std::endl;
        return EXIT_FAILURE;
    }

    // Treat any unhandled exception (e.g. in a thread) as a fatal error.
    Runtime::setUnhandledExceptionHandler([](Runtime::Exception&& exception)
                                          {
                                              Errors::fatalf("Unhandled runtime exception: %s\n",describeException(exception).c_str());
                                          });
    
    // Always enable debug logging for tests.
    Log::setCategoryEnabled(Log::Category::debug,true);
    
    int exitCode = commandMain(argc,argv);
    
    Runtime::collectGarbage();
    
    return exitCode;
}

