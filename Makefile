main:
	g++ -std=c++0x -o mmu main.cpp MM.cpp Random.cpp

clean:
	rm -rf mmu