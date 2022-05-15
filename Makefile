ifeq ($(board),uno)
        Target=uno
        MCU=atmega328p
        CPU_SPEED=16000000UL
	TTY_DEVICE=/dev/ttyACM0
	BAUD_RATE=115200
	CC=avr-gcc
	CPP=avr-g++
	AR=avr-ar
endif
ifeq ($(board),nano)
        Target=nano
        MCU=atmega328p
        CPU_SPEED=16000000UL
	TTY_DEVICE=/dev/ttyUSB0
	BAUD_RATE=57600
	CC=avr-gcc
	CPP=avr-g++
	AR=avr-ar
endif
ifeq ($(board),galileo)
        Target=galileo
	CC=gcc
	CPP=g++
	AR=ar
endif
PREFIX=../../Env/$(Target)
 
default:
ifeq ($(CPP),avr-g++)
	avr-g++ -L $(PREFIX)/lib -I $(PREFIX)/include -Wall -DF_CPU=$(CPU_SPEED) -Os -mmcu=$(MCU) -o main.elf main.c -larduino  
	avr-objcopy -O ihex -R .eeprom main.elf out.hex
endif
ifeq ($(CPP),g++)
	g++ -L /home/root/Env/lib -I /home/root/Env/include -Wall -Os -o SUC.elf main.c -larduino -pthread -DGALILEO
endif
upload:
ifeq ($(CPP),avr-g++)
	avrdude -c arduino -p m328p -b $(BAUD_RATE) -P $(TTY_DEVICE) -U flash:w:out.hex

endif
clean:
	@echo -n Cleaning ...
	$(shell rm *.elf 2> /dev/null)
	$(shell rm *.hex 2> /dev/null)
	$(shell rm *.o 2> /dev/null)
	@echo " done"

