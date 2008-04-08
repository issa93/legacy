# Makefile for serpent
ALGO_NAME := SERPENT

# comment out the following line for removement of serpent from the build process
BLOCK_CIPHERS += $(ALGO_NAME)


$(ALGO_NAME)_OBJ      := serpent.o serpent-sboxes-bitslice.o
$(ALGO_NAME)_TEST_BIN := main-serpent-test.o debug.o uart.o serial-tools.o \
                         serpent.o serpent-sboxes-bitslice.o nessie_bc_test.o \
                         nessie_common.o
$(ALGO_NAME)_NESSIE_TEST      := "nessie"
$(ALGO_NAME)_PEROFRMANCE_TEST := "performance"

