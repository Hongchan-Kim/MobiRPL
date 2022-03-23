CONTIKI = ../../..
APPS = powertrace deployment
CONTIKI_PROJECT = udp-sender udp-sink #app-rpl-collect-only 
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

ifdef PERIOD
CFLAGS=-DPERIOD=$(PERIOD)
endif

all: $(CONTIKI_PROJECT)

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
