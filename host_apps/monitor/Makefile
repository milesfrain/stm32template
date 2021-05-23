incDir = ../../common/inc
commonSrcDir = ../../common/src
target = monitor

commonSrcs = packet_utils.cpp software_crc.cpp
srcs = main.cpp $(addprefix $(commonSrcDir)/,$(commonSrcs))

# Currently setup in a slow simplified way where all dependencies
# are always rebuilt. This is fine for such a small project.

.PHONY : all clean

all : clean $(target)

clean :
	rm -f $(target)

$(target) : $(srcs) $(wildcard $(incDir)/*)
	g++ -Wall -Werror -g $(srcs) -I$(incDir) -o $@
