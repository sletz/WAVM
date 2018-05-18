add_executable(Assemble Assemble.cpp CLI.h)
target_link_libraries(Assemble Logging IR WAST WASM)
set_target_properties(Assemble PROPERTIES FOLDER Programs)

add_executable(Disassemble Disassemble.cpp CLI.h)
target_link_libraries(Disassemble Logging IR WAST WASM)
set_target_properties(Disassemble PROPERTIES FOLDER Programs)

#add_executable(Test Test.cpp CLI.h)
#target_link_libraries(Test Logging IR WAST Runtime)
#set_target_properties(Test PROPERTIES FOLDER Programs)

#add_executable(wavm wavm.cpp CLI.h)
#target_link_libraries(wavm Logging IR WAST WASM Runtime Emscripten)
#set_target_properties(wavm PROPERTIES FOLDER Programs)

# Faust/JACK/GTK specific needs

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)

include_directories(${GTK2_INCLUDE_DIRS})
link_directories(${GTK2_LIBRARY_DIRS})
add_definitions(${GTK2_CFLAGS_OTHER})

# We need dynamic_cast
add_compile_options(-frtti)

add_executable(faust-wavm faust-wavm.cpp wasm_dsp_aux.cpp emcc_dsp_aux.cpp CLI.h)
target_link_libraries(faust-wavm jack OSCFaust HTTPDFaust microhttpd)
target_link_libraries(faust-wavm ${GTK2_LIBRARIES})

# Coreaudio needed frameworks
target_link_libraries(faust-wavm "-framework CoreAudio")
target_link_libraries(faust-wavm "-framework AudioUnit")
target_link_libraries(faust-wavm "-framework CoreServices")
target_link_libraries(faust-wavm "-framework CoreMIDI")
target_link_libraries(faust-wavm "-framework CoreFoundation")
set_target_properties(faust-wavm PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")


target_link_libraries(faust-wavm Logging IR WAST WASM Runtime Emscripten)
set_target_properties(faust-wavm PROPERTIES FOLDER Programs)

add_executable(faustbench-wavm faustbench-wavm.cpp wasm_dsp_aux.cpp emcc_dsp_aux.cpp CLI.h)

target_link_libraries(faustbench-wavm Logging IR WAST WASM Runtime Emscripten)
set_target_properties(faustbench-wavm PROPERTIES FOLDER Programs)

add_executable(impulsewavm impulsewavm.cpp wasm_dsp_aux.cpp CLI.h)

target_link_libraries(impulsewavm Logging IR WAST WASM Runtime Emscripten)
set_target_properties(impulsewavm PROPERTIES FOLDER Programs)

if(ENABLE_RUNTIME)
	add_executable(Test Test.cpp CLI.h)
	target_link_libraries(Test Logging IR WAST Runtime ThreadTest)
	set_target_properties(Test PROPERTIES FOLDER Programs)

	add_executable(wavm wavm.cpp CLI.h)
	target_link_libraries(wavm Logging IR WAST WASM Runtime Emscripten ThreadTest)
	set_target_properties(wavm PROPERTIES FOLDER Programs)
endif()

