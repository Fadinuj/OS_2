CC = gcc
CFLAGS = -Wall -w -g -fprofile-arcs -ftest-coverage
LDFLAGS = -fprofile-arcs -ftest-coverage

# קבצי מקור
SERVER_SRC = drinks_bar.c
SUPPLIER_SRC = atom_supplier.c
REQUESTER_SRC = molecule_requester.c

# קבצים בינאריים
SERVER_BIN = drinks_bar
SUPPLIER_BIN = atom_supplier
REQUESTER_BIN = molecule_requester

# קובץ בדיקה
TEST_SCRIPT = testcase.sh

.PHONY: all test coverage clean

all: $(SERVER_BIN) $(SUPPLIER_BIN) $(REQUESTER_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SUPPLIER_BIN): $(SUPPLIER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(REQUESTER_BIN): $(REQUESTER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: all
	chmod +x $(TEST_SCRIPT)
	./$(TEST_SCRIPT)

coverage:
	gcov $(SERVER_SRC)
	gcov $(SUPPLIER_SRC)
	gcov $(REQUESTER_SRC)

clean:
	rm -f $(SERVER_BIN) $(SUPPLIER_BIN) $(REQUESTER_BIN) *.gcda *.gcno *.gcov
