SUBDIRS := EX1 EX2 EX3 EX4 EX5 EX6

.PHONY: all clean test coverage

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

test:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir test; \
	done

coverage:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir coverage; \
	done
