#
# Makefile for cc65 samples
#
# This Makefile requires GNU make
#

PRG	=	c64can
OBJ	=	spi.o can.o main.o

# Enter the target system here
SYS	= c64

CC65PATH = ~/packages/c64/cc65-2.10.1

CRT0   	= $(CC65PATH)/libsrc/$(SYS).o
CLIB	= $(CC65PATH)/libsrc/$(SYS).lib
CC	= $(CC65PATH)/src/cc65/cc65
CL	= $(CC65PATH)/src/cl65/cl65
AS	= $(CC65PATH)/src/ca65/ca65
LD	= $(CC65PATH)/src/ld65/ld65
C1541  	= c1541


# --------------------------------------------------------------------------
# Generic rules

.c.o:
	@echo $<
	$(CC) -Oirs -T  --codesize 500 -g -t $(SYS) -I $(CC65PATH)/include/ $<
	$(AS) $(basename $<).s

.s.o:
	@echo $<
	@$(AS) $(basename $<).s


# --------------------------------------------------------------------------
# Rules how to make each one of the binaries

all: $(PRG).prg


$(PRG).prg: 	$(CRT0) $(OBJ) $(CLIB)
	$(LD) -t $(SYS) -m $(basename $@).map -o $@ $^


# --------------------------------------------------------------------------
# Rule to make a disk with all samples. Needs the c1541 program that comes
# with the VICE emulator.

.PHONY:	disk
disk:  	samples.d64

samples.d64:	all
	@$(C1541) -format samples,AA  d64 $@ > /dev/null
	@for exe in $(EXELIST); do\
	    $(C1541) -attach $@ -write $$exe > /dev/null;\
	done;\
	for tgi in ../libsrc/$(SYS)*.tgi; do\
	    $(C1541) -attach $@ -write $$tgi > /dev/null;\
	done;

# --------------------------------------------------------------------------
# Cleanup rules

.PHONY:	clean
clean:
	$(RM) *~ *.map *.o *.s *.lbl $(PRG).prg

.PHONY:	zap
zap:	clean
	$(RM) $(EXELIST) samples.d64




