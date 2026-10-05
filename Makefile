# vaxum65 -- Glulxe for the MEGA65, built with Calypsi C (cc6502 5.18).

TARGET   = --target=mega65

# The game put on the disk. Its name on the disk is the host name in upper
# case (c1541 stores lower-case ASCII as PETSCII, the same bytes as upper-
# case ASCII), and that is the name the program opens.
GAME    ?= vanyar.ulx
GAME_DISKNAME = $(shell echo $(notdir $(GAME)) | tr a-z A-Z)

# The C stack. The toolchain default is 4096; lower it once the real
# high-water mark is known.
CSTACK  ?= 1024
# The heap, for the interpreter's own small allocations. (The Glulx stack
# is in bank 1; see src/mega65.h.)
HEAP    ?= 2048

DEFINES  = -DOS_MEGA65 -DGAME_FILE='"$(GAME_DISKNAME)"'
# --inline-on-matching-custom-text-section keeps a banked function from
# being inlined into near code, or into another bank.
CFLAGS   = $(TARGET) -O2 --speed --inline-on-matching-custom-text-section \
           -Isrc $(DEFINES)
ASFLAGS  = $(TARGET)
LDFLAGS  = $(TARGET) --output-format=prg
LINKFILE = src/mega65.scm

BUILD    = build

# Where each source file goes (see src/mega65.scm and src/bank.s):
#   PRG_SRC  the PRG below the bank slot: startup, loaders, what runs
#            before HICODE and the banks are loaded.
#   HI_SRC   near code above $8000, section hicode: the error handlers
#            and the Glk library. Banked files put some functions here
#            too, with NEAR_SECTION.
#   BANK_SRC in a bank, named in the file with BANK_SECTION.
# Left out: the Unix, Mac and iOS startup code, glulxdump.c (a host tool),
# float.c (float support is off) and debugger.c (empty unless VM_DEBUGGER
# is set). profile.c has only a stub of init_profile() unless VM_PROFILING
# is set.
PRG_SRC  = m65start m65load dma
HI_SRC   = main profile m65glk m65sys
BANK_SRC = vm exec operand funcs accel string search files gestalt osdepend \
           serial heap glkop gi_dispa

ASM      = $(patsubst %,$(BUILD)/%.s,$(PRG_SRC)) \
           $(patsubst %,$(BUILD)/%.fix.s,$(HI_SRC) $(BANK_SRC))
OBJS     = $(patsubst %,$(BUILD)/%.o,$(PRG_SRC) $(HI_SRC) $(BANK_SRC)) \
           $(BUILD)/bank.o

LINKDIR  = $(BUILD)/link
ELF      = $(LINKDIR)/vaxum65.elf
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
# layout constant or flag never leaves stale objects behind. Each C file is
# compiled to assembly first: banked and hicode files go through bankfix.py
# (which moves what cross-call lifted out of them into their own section),
# and bankcheck.py looks over all of it before the link.
$(BUILD)/%.s: src/%.c $(wildcard src/*.h) Makefile | $(BUILD)
	cc6502 $(CFLAGS) --assembly-source $@ -c $<

$(BUILD)/%.fix.s: $(BUILD)/%.s tools/bankfix.py
	python3 tools/bankfix.py $< $@

$(patsubst %,$(BUILD)/%.o,$(PRG_SRC)): $(BUILD)/%.o: src/%.c $(wildcard src/*.h) Makefile | $(BUILD)
	cc6502 $(CFLAGS) -c -o $@ $<

$(patsubst %,$(BUILD)/%.o,$(HI_SRC) $(BANK_SRC)): $(BUILD)/%.o: $(BUILD)/%.fix.s
	as6502 $(ASFLAGS) -o $@ $<

$(BUILD)/bank.o: src/bank.s Makefile | $(BUILD)
	as6502 $(ASFLAGS) -o $@ $<

$(BUILD)/bankcheck.ok: $(ASM) tools/bankcheck.py
	python3 tools/bankcheck.py $(ASM)
	touch $@

$(BUILD)/%.o: test/%.c $(wildcard src/*.h) Makefile | $(BUILD)
	cc6502 $(CFLAGS) -c -o $@ $<

# The list file is the memory map: take addresses from it, not from a
# hand-run link, since object order decides where every symbol lands.
# ln6502 -o names the ELF; the PRG is written beside it under the same
# stem, and --raw-multiple-memories writes hiCode.raw and bankStore.raw.
$(ELF): $(OBJS) $(LINKFILE) $(BUILD)/bankcheck.ok
	mkdir -p $(LINKDIR)
	cd $(LINKDIR) && ln6502 $(LDFLAGS) --raw-multiple-memories \
	    --cstack-size $(CSTACK) --heap-size $(HEAP) \
	    --list-file vaxum65.lst -o vaxum65.elf \
	    $(abspath $(LINKFILE)) $(abspath $(OBJS))

$(PRG): $(ELF)
	cp $(LINKDIR)/vaxum65.prg $@

# HICODE and BANKS: the raw images with their length in front, padded like
# the game (see m65load.c).
$(BUILD)/%.bin: $(ELF)
	python3 -c "import sys; d = open(sys.argv[1], 'rb').read(); \
	    open(sys.argv[2], 'wb').write(len(d).to_bytes(4, 'little') + d + bytes(512))" \
	    $(LINKDIR)/$*.raw $@

$(GAME_PAD): $(GAME) | $(BUILD)
	{ cat $<; head -c 512 /dev/zero; } > $@

# Built from scratch every time, so nothing stale is left on the image.
$(D81): $(PRG) $(BUILD)/hiCode.bin $(BUILD)/bankStore.bin $(GAME_PAD)
	rm -f $@
	c1541 -format "vaxum65,vx" d81 $@ \
	    -write $(PRG) autoboot.c65 \
	    -write $(BUILD)/hiCode.bin "hicode,s" \
	    -write $(BUILD)/bankStore.bin "banks,s" \
	    -write $(GAME_PAD) "$(notdir $(GAME)),s" >/dev/null

# The loader and attic RAM main memory on their own (test/loadtest.c),
# booted from a disk of their own. Prints a pass/fail list.
LOADTEST_OBJS = $(BUILD)/loadtest.o $(BUILD)/m65load.o $(BUILD)/dma.o
LOADTEST_D81  = $(BUILD)/loadtest.d81

$(BUILD)/loadtest.prg: $(LOADTEST_OBJS)
	ln6502 $(LDFLAGS) --cstack-size 4096 \
	    --list-file $(BUILD)/loadtest.lst -o $(BUILD)/loadtest.elf \
	    mega65-plain.scm $(LOADTEST_OBJS)

$(LOADTEST_D81): $(BUILD)/loadtest.prg $(GAME_PAD)
	rm -f $@
	c1541 -format "loadtest,vx" d81 $@ \
	    -write $(BUILD)/loadtest.prg autoboot.c65 \
	    -write $(GAME_PAD) "$(notdir $(GAME)),s" >/dev/null

loadtest: $(LOADTEST_D81)

runloadtest: $(LOADTEST_D81)
	xemu-xmega65 -besure -8 $(LOADTEST_D81)

# Banked code (src/bank.s) on its own: test/banktest*.c, with two banks of
# code for the slot. The link writes the program as banktest.prg and
# the banks as bankStore.raw; the banks go on the disk as BANKS, with their
# length in front and padded like the game.
BANKTEST_OBJS = $(BUILD)/banktest.o $(BUILD)/banktest_a.o \
                $(BUILD)/banktest_b.o $(BUILD)/bank.o $(BUILD)/m65load.o \
                $(BUILD)/dma.o
BANKTEST_DIR  = $(BUILD)/banktest
BANKTEST_D81  = $(BUILD)/banktest.d81

$(BUILD)/banktest%.o: test/banktest%.c test/banktest.h $(wildcard src/*.h) Makefile | $(BUILD)
	cc6502 $(CFLAGS) -Itest -c -o $@ $<

$(BANKTEST_DIR)/banktest.prg: $(BANKTEST_OBJS) test/banktest.scm
	mkdir -p $(BANKTEST_DIR)
	cd $(BANKTEST_DIR) && ln6502 $(LDFLAGS) --raw-multiple-memories \
	    --cstack-size 512 --list-file banktest.lst -o banktest.elf \
	    $(abspath test/banktest.scm) $(abspath $(BANKTEST_OBJS))

$(BANKTEST_DIR)/banks.bin: $(BANKTEST_DIR)/banktest.prg
	python3 -c "import sys; d = open(sys.argv[1], 'rb').read(); \
	    open(sys.argv[2], 'wb').write(len(d).to_bytes(4, 'little') + d + bytes(512))" \
	    $(BANKTEST_DIR)/bankStore.raw $@

$(BANKTEST_D81): $(BANKTEST_DIR)/banktest.prg $(BANKTEST_DIR)/banks.bin
	rm -f $@
	c1541 -format "banktest,vx" d81 $@ \
	    -write $(BANKTEST_DIR)/banktest.prg autoboot.c65 \
	    -write $(BANKTEST_DIR)/banks.bin "banks,s" >/dev/null

banktest: $(BANKTEST_D81)

runbanktest: $(BANKTEST_D81)
	xemu-xmega65 -besure -8 $(BANKTEST_D81)

clean:
	rm -rf $(BUILD)

.PHONY: all run objs loadtest runloadtest banktest runbanktest clean
