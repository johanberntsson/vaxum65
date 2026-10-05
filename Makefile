# vaxum65 -- Glulxe for the MEGA65, built with Calypsi C (cc6502 5.18).

TARGET   = --target=mega65

# The game put on the disk. Its name on the disk is the host name in upper
# case (c1541 stores lower-case ASCII as PETSCII, the same bytes as upper-
# case ASCII), and that is the name the program opens.
GAME    ?= vanyar.ulx
GAME_DISKNAME = $(shell echo $(notdir $(GAME)) | tr a-z A-Z)

# The interpreter includes glk.h, gi_dispa.h and gi_blorb.h. Until there is
# a MEGA65 Glk layer of our own, they come from a CheapGlk checkout.
GLKDIR  ?= ../cheapglk

# The C stack. The toolchain default is 4096; lower it once the real
# high-water mark is known.
CSTACK  ?= 4096

DEFINES  = -DOS_MEGA65 -DGAME_FILE='"$(GAME_DISKNAME)"'
CFLAGS   = $(TARGET) -O2 --speed -Isrc -I$(GLKDIR) $(DEFINES)
ASFLAGS  = $(TARGET)
LDFLAGS  = $(TARGET) --output-format=prg
LINKFILE = src/mega65.scm

BUILD    = build

# The interpreter core, plus the MEGA65 layer (dma, m65load). Left out: the
# Unix, Mac and iOS startup code (unixstrt.c, unixautosave.c, macstart.c)
# and glulxdump.c, which is a separate host tool. debugger.c and profile.c
# compile to nothing unless VM_DEBUGGER / VM_PROFILING are set in glulxe.h.
CORE     = main files vm exec funcs operand string glkop heap serial \
           search accel float gestalt osdepend profile debugger \
           dma m65load
SRCS     = $(addprefix src/,$(addsuffix .c,$(CORE)))
ASRCS    = $(wildcard src/*.s)
OBJS     = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS)) \
           $(patsubst src/%.s,$(BUILD)/%.o,$(ASRCS))

ELF      = $(BUILD)/vaxum65.elf
# The MEGA65 ROM autoboots a file called autoboot.c65.
PRG      = $(BUILD)/autoboot.c65
D81      = $(BUILD)/vaxum65.d81

# The game as it goes on the disk: padded with 512 zero bytes, because the
# Kernal reports EOF on a SEQ file 256 bytes before its real end. The loader
# reads the length the game header gives, never up to EOF.
GAME_PAD = $(BUILD)/game.pad

all: $(D81)

run: $(D81)
	xemu-xmega65 -besure -8 $(D81)

# Compile every source without linking, and keep going past failures:
# the quickest way to see what the port still has to fix.
objs: $(OBJS)

$(BUILD):
	mkdir -p $(BUILD)

# Every object depends on every header and on this Makefile, so a changed
# layout constant or flag never leaves stale objects behind.
$(BUILD)/%.o: src/%.c $(wildcard src/*.h) Makefile | $(BUILD)
	cc6502 $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: src/%.s $(wildcard src/*.h) Makefile | $(BUILD)
	as6502 $(ASFLAGS) -o $@ $<

$(BUILD)/%.o: test/%.c $(wildcard src/*.h) Makefile | $(BUILD)
	cc6502 $(CFLAGS) -c -o $@ $<

# The list file is the memory map: take addresses from it, not from a
# hand-run link, since object order decides where every symbol lands.
$(ELF): $(OBJS) $(LINKFILE)
	ln6502 $(LDFLAGS) --cstack-size $(CSTACK) \
	    --list-file $(BUILD)/vaxum65.lst -o $@ $(LINKFILE) $(OBJS)

# ln6502 -o names the ELF; the PRG is written beside it under the same stem.
$(PRG): $(ELF)
	cp $(BUILD)/vaxum65.prg $@

$(GAME_PAD): $(GAME) | $(BUILD)
	{ cat $<; head -c 512 /dev/zero; } > $@

# Built from scratch every time, so nothing stale is left on the image.
$(D81): $(PRG) $(GAME_PAD)
	rm -f $@
	c1541 -format "vaxum65,vx" d81 $@ \
	    -write $(PRG) autoboot.c65 \
	    -write $(GAME_PAD) "$(notdir $(GAME)),s" >/dev/null

# The loader and attic RAM main memory on their own (test/loadtest.c),
# booted from a disk of their own. Prints a pass/fail list.
LOADTEST_OBJS = $(BUILD)/loadtest.o $(BUILD)/m65load.o $(BUILD)/dma.o
LOADTEST_D81  = $(BUILD)/loadtest.d81

$(BUILD)/loadtest.prg: $(LOADTEST_OBJS) $(LINKFILE)
	ln6502 $(LDFLAGS) --cstack-size $(CSTACK) \
	    --list-file $(BUILD)/loadtest.lst -o $(BUILD)/loadtest.elf \
	    $(LINKFILE) $(LOADTEST_OBJS)

$(LOADTEST_D81): $(BUILD)/loadtest.prg $(GAME_PAD)
	rm -f $@
	c1541 -format "loadtest,vx" d81 $@ \
	    -write $(BUILD)/loadtest.prg autoboot.c65 \
	    -write $(GAME_PAD) "$(notdir $(GAME)),s" >/dev/null

loadtest: $(LOADTEST_D81)

runloadtest: $(LOADTEST_D81)
	xemu-xmega65 -besure -8 $(LOADTEST_D81)

clean:
	rm -rf $(BUILD)

.PHONY: all run objs loadtest runloadtest clean
