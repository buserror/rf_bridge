SHELL			= /bin/bash

# default frequency
FREQ 			= 16000000
O 				= build

TARGET			= rf_bridged
DESTDIR 		?= /
SOV				= 1
VERSION			= 0.90
PKG				= 1
DESC			= 433Mhz to MQTT Bridge (Software part)

ifeq (${V},)
E 				= @
else
E 				=
endif

AVR_SRC			:= $(wildcard avr/at*.c)
AVR_OBJ			:= $(patsubst avr/%, ${O}/%, ${AVR_SRC:%.c=%.axf})
EXTRA_CFLAGS	+= -Ishared

ifneq ($(SIMAVR),)
EXTRA_CFLAGS	+= -DSIMAVR
EXTRA_CFLAGS	+= -I$(SIMAVR)/simavr/sim/avr/
endif

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
	${E}${CC} -o $@ -MMD -std=gnu99 -g -Og ${EXTRA_CFLAGS} \
		$^ -Wall \
		-DMQTT -lmosquitto

deb:
	rm -rf /tmp/deb
	make clean && make all && make install DESTDIR=/tmp/deb/
	mkdir -p $(O)/debian; (cd $(O)/debian; \
	fpm -s dir -t deb -C /tmp/deb -n $(TARGET) -v $(VERSION) --iteration $(PKG) \
		--description "$(DESC)" \
		-d "libmosquito1 (>= 1.4.0)" \
	)

install: ${O}/rf_bridged
	mkdir -p $(DESTDIR)/etc/ $(DESTDIR)/usr/bin $(DESTDIR)/usr/share/$(TARGET) && \
	install -s ${O}/rf_bridged $(DESTDIR)/usr/bin/ && \
		cp rf_bridged.conf $(DESTDIR)/etc/ && \
		cp ${O}/*.axf $(DESTDIR)/usr/share/$(TARGET)/

-include ${wildcard ${O}/*.d}
