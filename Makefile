SRCS := $(shell find ./src -name *.c)
HEADERS := $(shell find ./src -name *.h)

FS360: $(SRCS) $(HEADERS)
	gcc -g -o $@ $(SRCS)