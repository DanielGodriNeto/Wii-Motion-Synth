#---------------------------------------------------------------------------------
# Wii Motion Synth - Makefile
#---------------------------------------------------------------------------------

APP_NAME    := WiiMotionSynth
BUILD_DIR   := build
SRCS        := main.c

DEVKITPRO   := C:/devkitPro
DEVKITPPC   := C:/devkitPro/devkitPPC

PREFIX      := $(DEVKITPPC)/bin/powerpc-eabi-
CC          := $(PREFIX)gcc

# Caminho do executavel elf2dol
ELF2DOL     := C:/devkitPro/tools/bin/elf2dol.exe

INCLUDES    := -I$(DEVKITPRO)/libogc/include \
               -I$(DEVKITPRO)/libogc/include/wiiuse

LIBPATHS    := -L$(DEVKITPRO)/libogc/lib/wii \
               -L$(DEVKITPPC)/powerpc-eabi/lib

CFLAGS      := -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float -O2 -Wall $(INCLUDES)
# Inclusao das bibliotecas do Wiimote (wiiuse) e Bluetooth (bte)
LIBS        := -lwiiuse -lbte -lasnd -lfat -logc -lm
LDFLAGS     := $(LIBPATHS) -mrvl -Wl,-gc-sections $(LIBS)

all: $(BUILD_DIR)/$(APP_NAME).dol

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/$(APP_NAME).elf: $(SRCS) | $(BUILD_DIR)
	@echo "Compilando $(APP_NAME).elf..."
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(APP_NAME).dol: $(BUILD_DIR)/$(APP_NAME).elf
	@echo "Convertendo para executavel Wii (.dol)..."
	$(ELF2DOL) $< $@
	@echo "Sucesso! Arquivo gerado em $@"

clean:
	@rm -rf $(BUILD_DIR)

.PHONY: all clean