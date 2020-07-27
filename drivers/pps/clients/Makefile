#
# Makefile for PPS clients.
#

obj-$(CONFIG_PPS_CLIENT_KTIMER)	+= pps-ktimer.o
obj-$(CONFIG_PPS_CLIENT_LDISC)	+= pps-ldisc.o
obj-$(CONFIG_PPS_CLIENT_GPIO)	+= pps-gpio.o

ifeq ($(CONFIG_PPS_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
