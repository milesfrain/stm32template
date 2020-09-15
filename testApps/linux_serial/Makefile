incDir = ../../common/inc
target = spamPackets

$(target) : main.cpp $(incDir)/packets.h
	g++ -g $< -I$(incDir) -o $@

.PHONY : run

run: $(target)
	./$(target)
