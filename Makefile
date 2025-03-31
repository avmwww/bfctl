#
# Makefile for bfctl project
#

CC=$(CROSS_COMPILE)gcc
STRIP=$(CROSS_COMPILE)strip

TARGET = bfctl

SRCDIR = src
OBJDIR = obj
INCDIR = include

LIBRARY=../library

PROGOPT=$(LIBRARY)/progopt
UTILS=$(LIBRARY)/utils

ifeq ($(findstring mingw,$(CROSS_COMPILE)),)
	TARGET_OS := Linux
else
	TARGET_OS := MINGW
endif

SRCMISC = progopt.c failure.c serial.c zalloc.c dump_hex.c

SRCS =  \
	bfctl.c \
	cmd_arg.c \
	msp.c \
	msp_cmd.c \
	esc_boot.c \
	esc4way.c \
	crc.c \
	bf.c \

SRCS += $(SRCMISC)

OBJS = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))

CFLAGS ?= $(C_FLAGS)
CFLAGS += -I$(INCDIR) -I$(PROGOPT) -I$(UTILS) -I$(MENU)
CFLAGS += -I../include -I.
CFLAGS += -Wall
CFLAGS += -D_GNU_SOURCE -g -Og

ifeq ($(TARGET_OS),MINGW)
CFLAGS += -I$(LIBRARY)/serial/windows
TARGET_POSTFIX = ".exe"
else
CFLAGS += -I$(LIBRARY)/serial/posix
endif

LDFLAGS ?= $(LD_FLAGS)

all: $(OBJDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@
	$(STRIP) -s $@$(TARGET_POSTFIX)

clean:
	rm -rf $(TARGET) $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o : %.c $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

# VPATH
ifeq ($(TARGET_OS),MINGW)
vpath serial.c $(LIBRARY)/serial/windows
else
vpath serial.c $(LIBRARY)/serial/posix
endif

vpath %.c $(SRCDIR)
vpath %.h $(INCDIR)

vpath %.c $(PROGOPT) $(UTILS) $(INIH)

vpath %.o $(OBJDIR)

