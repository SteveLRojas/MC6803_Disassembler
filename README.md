# MC6803_Disassembler
Simple disassembler tool for 6800 and 6803 processors.  
  
This tool was designed to help reverse engineer the TRS-80 model MC-10 ROM. It is a very simple tool and does not resolve indexed jumps, but it does allow the user to specify additional entry points to be examined.  
Rather than producing an assembly listing the program lists the address, op code name, and arguments of each instruction.  
The output listing is printed in order of increasing address, skipping addresses that were not examined.  
Although the tool was specifically designed for the MC-10 ROM, the ROM file can be of any size (up to 64KB) and is aligned with the end of the address space.  
