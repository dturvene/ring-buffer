# simple Makefile
# create dependency rules in hidden directory in $DEPDIR
# include them

# -*- mode: makefile;-*-
# simple Makefile with dependency rules

DOCTITLE = "A Simple Ringbuffer In C"

CC = gcc
CFLAGS = -g -Wall -DLOG_EVENT
RM = rm -f
LIBS = -pthread

OBJS := ringbuffer.o logevt.o
BINS := ringbuffer

all: $(BINS)

# executable: object dependencies
ringbuffer: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# compile object file and create dependency file
%.o: %.c
	$(CC) $(CFLAGS) -c $<
	$(CC) -MM $(CFLAGS) $< > $*.d

# https://www.gnu.org/software/make/manual/html_node/Include.html
# include all the dependency rule files, ignore if none exist
-include $(OBJS:.o=.d)

test:
	@echo validation test
	@time ./ringbuffer -t 1 > /tmp/rb.logs
	@echo "simple stress"
	@time ./ringbuffer -t 2 >> /tmp/rb.logs
	@echo large stress using spinlock for critical section
	@time ./ringbuffer -t 3 -c 10000000 >> /tmp/rb.logs
	@echo large stress using pthread mutex for critical section
	@time ./ringbuffer -t 3 -c 10000000 -m >> /tmp/rb.logs

# rule to convert a markdown doc to html
# be sure html files are in .gitignore
%.html :: %.md
	@echo "view with firefox $@"
	pandoc --metadata pagetitle=${DOCTITLE} -f markdown -s $< -o $@

# remove all generated files
clean:
	$(RM) *.o
	$(RM) *.d
	$(RM) *.html
	$(RM) $(BINS)

.PHONY: clean
