CC = gcc
CFLAGS = -Wall -Wextra -g -fprofile-arcs -ftest-coverage
LDFLAGS = -fprofile-arcs -ftest-coverage

# קבצי מקור
SERVER_SRC = drinks_bar.c
CLIENT_SRC = molecule_requester.c
SUPPLIER_SRC = atom_supplier.c

# קבצים בינאריים
SERVER_BIN = drinks_bar
CLIENT_BIN = molecule_requester
SUPPLIER_BIN = atom_supplier

TEST_SCRIPT = testcase.sh

.PHONY: all test coverage clean

all: $(SERVER_BIN) $(CLIENT_BIN) $(SUPPLIER_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SUPPLIER_BIN): $(SUPPLIER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: all
	chmod +x $(TEST_SCRIPT)
	./$(TEST_SCRIPT)

coverage:
	gcov $(SERVER_SRC)
	gcov $(CLIENT_SRC)
	gcov $(SUPPLIER_SRC)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) $(SUPPLIER_BIN) *.gcda *.gcno *.gcov
