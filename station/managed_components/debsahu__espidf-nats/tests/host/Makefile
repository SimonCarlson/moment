CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I../../include -I. -Ifake_esp_idf -DHOST_TEST -g
LDFLAGS = -lcjson

.PHONY: all clean test

all: test_utils test_parsers

FAKE_HEADERS = $(wildcard fake_esp_idf/*.h fake_esp_idf/**/*.h)

test_utils: test_utils.cpp $(FAKE_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test_parsers: test_parsers.cpp $(FAKE_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test: test_utils test_parsers
	@echo ""
	@echo "=== Running test_utils ==="
	./test_utils
	@echo ""
	@echo "=== Running test_parsers ==="
	./test_parsers
	@echo ""
	@echo "All tests complete."

clean:
	rm -f test_utils test_parsers
