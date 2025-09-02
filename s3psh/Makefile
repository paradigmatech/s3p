override CFLAGS += -Wall

# If libreadline is not available pass DISABLE_READLINE=1 to make
ifndef DISABLE_READLINE
override LDLIBS += -lreadline
override CFLAGS += -DUSE_READLINE
endif

APP = s3psh

INCLUDES = -I../include

OBJS = s3psh_utils.o ser.o s3psh.o
OBJS += ../src/s3p.o
OBJS += ../src/value.o
OBJS += ../src/cobs.o
OBJS += ../src/crc16.o

.PHONY: all clean cleanall

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -f $(OBJS)

cleanall: clean
	rm -f $(APP)

