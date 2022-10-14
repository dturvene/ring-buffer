# simple Makefile
CC=gcc
DEBUGFLAGS=-g
CFLAGS=$(DEBUGFLAGS)

BINS := \
	ringbuffer

RM=rm -f

all: $(BINS)

%: %.o
	echo ".o"
	$(CC) $(CFLAGS) $(LIBS) $? -o $@

%.o: %.c
	echo ".c"
	$(CC) $(CFLAGS) -c $<

# local convert a markdown doc to html
%.html :: %.md
	pandoc -f markdown -s $< -o /tmp/$@

clean: 
	$(RM) *.o *.lst *.i
	$(RM) $(BINS)
	$(RM) *.dump

.PHONY: clean
