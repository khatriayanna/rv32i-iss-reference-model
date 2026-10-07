CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

rv32i_iss: rv32i_iss.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

test: rv32i_iss
	./rv32i_iss examples/prog.bin trace.txt

clean:
	rm -f rv32i_iss trace.txt
