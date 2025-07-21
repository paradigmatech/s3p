#OPT_CFLAGS = -O2
OPT_CFLAGS = -O0 -g -Wall
#OPT_CFLAGS = -O0 -g
CFLAGS = $(OPT_CFLAGS)
# If libreadline is not available, comment this line and undef
# USE_READLINE in client.c
LFLAGS = -lreadline
CC = gcc
CXX = g++
LD = g++
APP = s3psh

INCLUDES = -I../include

OBJS = s3psh_utils.o ser.o s3psh.o
OBJS += ../src/s3p.o
OBJS += ../src/value.o
OBJS += ../src/cobs.o
OBJS += ../src/crc16.o

all: $(APP)

$(APP): $(OBJS)
	$(LD) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -f $(OBJS)

cleanall: clean
	rm -f $(APP)

