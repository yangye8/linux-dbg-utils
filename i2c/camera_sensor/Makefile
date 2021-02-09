COMPILE_OPTS =		$(INCLUDES) -I. -O2 -DSOCKLEN_T=socklen_t -DNO_SSTREAM=1 -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64  -fPIC -D ALLOW_RTSP_SERVER_PORT_REUSE=1 
C_COMPILER =		$(CC)
C_FLAGS =		$(COMPILE_OPTS)
LDFLAGS= -pthread -lpthread 
SRCS = sccb_tool.c

OBJS = $(SRCS:.c=.o)
MAIN = sccb

.PHONY: depend clean

all:    $(MAIN)
	@echo  sccb has been compiled

$(MAIN): $(OBJS) 
	$(C_COMPILER) $(C_FLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS) $(LDFLAGS)
.c.o:
	$(C_COMPILER) $(CFLAGS) $(INCLUDES) -c $<  -o $@
clean:
	$(RM) *.o *~ $(MAIN)
depend: $(SRCS)
	makedepend $(INCLUDES) $^
