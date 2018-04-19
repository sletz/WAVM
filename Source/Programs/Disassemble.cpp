#include "Inline/BasicTypes.h"
#include "Inline/Timing.h"
#include "CLI.h"
#include "WAST/WAST.h"
#include "WASM/WASM.h"

int main(int argc,char** argv)
{
	if(argc != 3)
	{
		std::cerr <<  "Usage: Disassemble in.wasm out.wast" << std::endl;
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];
	const char* outputFilename = argv[2];
	
	// Load the WASM file.
	IR::Module module;
	if(!loadBinaryModule(inputFilename,module)) { return EXIT_FAILURE; }

	// Print the module to WAST.
	Timing::Timer printTimer;
	const std::string wastString = WAST::print(module);
	Timing::logTimer("Printed WAST",printTimer);
	
	// Write the serialized data to the output file.
	std::ofstream outputStream(outputFilename);
	outputStream.write(wastString.data(),wastString.size());
	outputStream.close();

	return EXIT_SUCCESS;
}

int main(int argc,char** argv)
{
    int result = 0;
    try
    {
        Runtime::catchRuntimeExceptions(
                                        [&]
                                        {
                                            result = commandMain(argc,argv);
                                        },
                                        [&](Runtime::Exception&& exception)
                                        {
                                            std::cerr << "Runtime exception: " << describeException(exception) << std::endl;
                                            result = EXIT_FAILURE;
                                        });
    }
    catch(IR::ValidationException exception)
    {
        std::cerr << "Failed to validate module: " << exception.message << std::endl;
        result = EXIT_FAILURE;
    }
    catch(Serialization::FatalSerializationException exception)
    {
        std::cerr << "Fatal serialization exception: " << exception.message << std::endl;
        result = EXIT_FAILURE;
    }
    
    return result;
}
