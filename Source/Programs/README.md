# Running Faust DSP code with WAVM 

Faust DSP programs compiled as wasm modules can be run using the WAVM WebAssembly Virtual machine. 

## faust-wavm

Assuming you have installed the Faust2 branch, compile WAVM by following the standard procedure explained [here](https://github.com/AndrewScheidecker/WAVM). An additional **wavm-faust** tool using JACK for audio and GTK for the GUI will be compiled:

`faust-wavm [-nvoices N] [-midi] [-osc] [-httpd] foo.wasm`

Here are the available options:

- `-nvoices N to start the DSP in polyphonic mode with N voices`
- `-midi to activate MIDI control`
- `-osc to activate OSC control`
- `-httpd to activate HTTPD control`

With the JACK server running, the program can simply be tested by compiling a wasm module from a Faust source program with `faust -lang wasm foo.dsp -o foo.wasm`, then launching it with `wavm-faust foo.wasm`.

## Known issues

Polyphonic mode is not yet working correctly. 

## faustbench-wavm

Benckmarking the Faust generated wasm code can be done using the **faustbench-wavm** tool:

`faustbench-wavm foo.wasm`
