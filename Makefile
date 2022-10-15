# simple Makefile
CC=gcc
DEBUGFLAGS=-g
CFLAGS=$(DEBUGFLAGS)
LIBS=-pthread

BINS := \
	ringbuffer

RM=rm -f

all: $(BINS)

# executable: object dependencies
ringbuffer: ringbuffer.o logevt.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# gmake pattern rule
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# rule to convert a markdown doc to html
# be sure html files are in .gitignore
%.html :: %.md
	@echo "view with firefox $@"
	pandoc -f markdown -s $< -o $@

# remove all generated files
clean: 
	$(RM) *.o
	$(RM) $(BINS)

.PHONY: clean
