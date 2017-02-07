SHELL=/bin/bash

# default frequency
FREQ = 16000000
O = build

ifeq (${V},)
E = @
else
E = 
endif

AVR_SRC		:= $(wildcard avr/at*.c)
AVR_OBJ		:= $(patsubst avr/%, ${O}/%, ${AVR_SRC:%.c=%.axf})
EXTRA_CFLAGS += -Ishared

all :  ${O} ${AVR_OBJ} ${O}/rf_bridged

${O}: 
	${E}mkdir -p build
		
${O}/%.hex: ${O}/%.axf
		${E}avr-objcopy -j .text -j .data -O ihex ${<} ${@}
${O}/%.s: ${O}/%.axf
		${E}avr-objdump -S -j .text -j .data -j .bss -d  ${<} > ${@}
${O}/%.axf: avr/%.c 
		${E}echo AVR-CC ${<}
		${E}part=${shell basename ${<}} ; part=$${part/_*}; \
		avr-gcc -MMD -Wall -gdwarf-2 -Os -std=gnu99 ${EXTRA_CFLAGS} \
				-mmcu=$$part \
				-DF_CPU=${FREQ} \
				-mcall-prologues -fno-inline-small-functions \
				-ffunction-sections -fdata-sections \
				-Wl,--relax,--gc-sections \
				-Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000 \
				${^} -o ${@}
		${E}avr-size ${@}|sed '1d'

clean:
	rm -rf ${O}

${O}/atmega2560_rf_bridge.axf: avr/rf_bridge_uart.c
${O}/atmega2560_rf_bridge.axf: avr/rf_bridge_common.c

${O}/atmega328p_rf_bridge.axf: avr/rf_bridge_uart.c
${O}/atmega328p_rf_bridge.axf: avr/rf_bridge_common.c

rfbridge: ${O}/atmega2560_rf_bridge.axf
	avrdude -p m2560 -c wiring -P /dev/ttyACM0 -D -Uflash:w:$^

${O}/rf_bridged: ${wildcard src/*.c}
	${E}echo CC ${^}
	${E}${CC} -o $@ -MMD -std=gnu99 -Og ${EXTRA_CFLAGS} \
		$^ -Wall \
		-DMQTT -lmosquitto


-include ${wildcard ${O}/*.d}
