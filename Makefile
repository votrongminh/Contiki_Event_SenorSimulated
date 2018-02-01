ifndef CONTIKI
  CONTIKI = /home/user/NES/contiki
endif

ifndef TARGET
TARGET=sky
endif

all: EventDetection.sky

upload: EventDetection.upload

simulation:
	java -jar $(CONTIKI)/tools/cooja/dist/cooja.jar -contiki=$(CONTIKI)

CONTIKI_WITH_IPV4 = 1
CONTIKI_WITH_RIME = 1
include $(CONTIKI)/Makefile.include
