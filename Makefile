COMPILER = g++
OUTPUT_FILE = parser_calculator
SRC = parser.cpp
OPTIONS = -O3 -std=c++17 -Wall -L/usr/include/boost

LIBS =

$(OUTPUT_FILE) : $(SRC)
		$(COMPILER) $(SRC) $(OPTIONS) -o $(OUTPUT_FILE) $(LIBS)
