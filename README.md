# UK101RE
Grant Searle's micro UK101 replica emulator

![UK101RE](https://github.com/ea4eoz/uk101re/blob/main/photos/MicroUK101.jpg?raw=true)

This is an emulator of my [Grant Searle's micro UK101](http://searle.x10host.com/uk101/uk101.html) single board computer for Unix-like systems. It is a fork of my [Apple I replica emulator](https://github.com/ea4eoz/aire) but shares a lot of code because both computers are very similar.

## Compiling

Just type 'make' or 'gmake', depending on your system.

## Getting the EPROM file

You must download the EPROM image from Grant Searle's web page, at this location:

[http://searle.x10host.com/uk101/rom.zip](http://searle.x10host.com/uk101/rom.zip)

Just unzip the file and place the all.rom file in the same directory where the uk101re executable file is.

## Usage:

Just execute 'uk101re'. There are some command line options:
<pre>
&nbsp;&nbsp;&nbsp;&nbsp;-h,         --help          Show help.
&nbsp;&nbsp;&nbsp;&nbsp;-v,         --version       Show UK101RE version.
&nbsp;&nbsp;&nbsp;&nbsp;-t,         --turbo         Enable turbo mode.
&nbsp;&nbsp;&nbsp;&nbsp;-r romfile, --rom romfile   Specify ROM file.
</pre>

__Turbo__: The micro UK101 replica runs 1 MHz and the emulator tries to match that speed. Using the Turbo option makes the emulator to run as fast as possible.

__Rom__: This option is used to specify an alternate rom file. The entire rom is mapped into addresses 0x8000 - 0xFFFF except for the segment 0xF000 - 0xF7FF where the ACIA is mapped.

## Loading software

You can type your programs in the terminal, just as you would do with a real micro UK101 single board computer. You can also paste text into the console to enter large programs. Alternatively, you can use common text files (text files with commands to type into the emulator) as argument when launching the emulator, for example, type:
<pre>
&nbsp;&nbsp;&nbsp;&nbsp;./uk101re Software/BasicHelloWorld.txt
</pre>
to execute a simple Hello World basic program. Note that while loading a text file, turbo option is enabled.

While running, you can use these keyboard shortcuts:
<pre>
&nbsp;&nbsp;&nbsp;&nbsp;<b>Ctrl-R</b>: Resets CPU. RAM content is kept.
&nbsp;&nbsp;&nbsp;&nbsp;<b>Ctrl-X</b>: Quits emulator.
</pre>
The first thing you will see when launching the emulator is:
<pre>
<b>Micro UK101 C/W/M?</b>
</pre>

Press <b>C</b> for a cold restart. RAM will be erased. After pressing C, you will be asked for ram size and terminal width. Just press enter for automatic / default values.

Press <b>W</b> for a warm restart. RAM content will be preserved. Use ONLY after a CPU reset.

Press <b>M</b> to enter CEGMON monitor software.

Use capital letters for all comands, both in CEGMON and BASIC.

While running a BASIC program, you can use Ctrl-C to stop the program. Use the CONT command to continue it.

## Provided Software

In the Software directory you can find some programs:

<b>Acey-Deucy.txt</b>: A cards game from UK101 user manual in BASIC.

<b>Decimal2Binary.txt</b>: An improved decimal to binary converter based on the one at UK101 user manual in BASIC.

<b>Fahrenheit-Celsius.txt</b>: A small utility to convert Celsius to Fahrenheit temperatures nad vice-versa from the UK101 user manual in BASIC.

<b>HelloWorld.txt</b>: Simple Hello World program in BASIC.

<b>NumberGuess.txt</b>: A simple guess-the-number game in BASIC

<b>PowersOfTwo.txt</b>: Displays the powers of two in BASIC.

<b>PrimeNumbers.txt</b>: Simple program in BASIC that calculate prime numbers.

<b>Sinewave.txt</b>: A program in BASIC that draws a sinewave in the terminal.

## Documentation

Some useful documentation:

<b>Grant Searle's micro UK101 web page</b>: [http://searle.x10host.com/uk101/uk101.html](http://searle.x10host.com/uk101/uk101.html)

<b>UK101 manual<b>: [https://www.flaxcottage.com/UK101/UK101Manual.pdf](https://www.flaxcottage.com/UK101/UK101Manual.pdf)

<b>CEGMON manual<b>: [https://www.flaxcottage.com/UK101/CEGMON/CEGMON_manual.pdf](https://www.flaxcottage.com/UK101/CEGMON/CEGMON_manual.pdf)

<b>6502 Instruction set</b>: [https://www.masswerk.at/6502/6502_instruction_set.html](https://www.masswerk.at/6502/6502_instruction_set.html)

## License

The source files of the micro UK101 Replica Emulator in this repository are made available under the GPLv3 license. The example files under the Software directory are included for convenience only and all have their own license, and are not part of the micro UK101 Replica Emulator.
