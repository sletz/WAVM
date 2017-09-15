# Running Faust DSP code with WAVM 

Faust DSP programs compiled as wasm modules can be run using the WAVM WebAssembly Virtual machine. 

## faust-wavm

Assuming you have installed the Faust2 branch, compile WAVM by following the standard procedure explained [here](https://github.com/AndrewScheidecker/WAVM). An additional **faust-wavm** tool using JACK for audio and GTK for the GUI will be compiled:

`faust-wavm [-emcc] [-nvoices N] [-midi] [-osc] [-httpd] foo.wasm`

Here are the available options:

- `-nvoices N to start the DSP in polyphonic mode with N voices`
- `-midi to activate MIDI control`
- `-osc to activate OSC control`
- `-httpd to activate HTTPD control`

With the JACK server running, the program can simply be tested by compiling a wasm module from a Faust source program with `faust -lang wasm foo.dsp -o foo.wasm`, then launching it with `faust-wavm foo.wasm`.

## faustbench-wavm

Benchmarking the Faust generated wasm code can be done using the **faustbench-wavm** tool:

`faustbench-wavm [-emcc] [-run <num>] foo.wasm`

## impulsewavm

This **impulsewavm** is designed to be deployed in the Faust2 [impulse-tests](https://github.com/grame-cncm/faust/tree/faust2/tests/impulse-tests) compiler backend test infrastructure. 

`impulsewavm foo.wasm` runs the impulse-test on foo.wasm code.

All impulse-tests have been validated september the 14th.

