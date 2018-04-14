# Pipelined Behavioral Simulator #
Written by Drew Wilken and Nathan Taylor

A behavioral simulator written for the UST-3400 ISA, that implements a pipelined design rather than a single cycle design. It currently takes in machine code generated from the assembler and runs instructions. 

**How the pipeline works**
In this simulator, the pipeline design helps increase throughput of a program. In software, it is a much more complex program, so it actually runs a bit slower than the simple single-cycle design that we implemented before, but in theory and hardware, this design would increase throughput but a large percentage. 

It works by breaking instructions into stages:
* Instruction fetch
* Instruction decode
* Execution
* Memory
* Writeback

By breaking the instructions into small actions, the cycle time becomes much shorter. 

But, this is not alone what happens. To actually get the benefit of increased through-put, we also did more pipelining to basically load the pipeline with up to 4 instructions at a time. Therefore, due to the cycles being much faster, instructions get retired much faster.

## USAGE ##

To use the pipeline simulator:
* Download the project
* Compile
* Run, and type the name of your machine code file generated from the assembler (this can be downloaded from my page as well)
