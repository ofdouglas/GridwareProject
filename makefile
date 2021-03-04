include nanopb/extra/nanopb.mk

SRC_DIR := src
INC_DIR := inc
BUILD_DIR := build
ROOT_DIR := $(CURDIR)

CPP_SRC = $(wildcard $(SRC_DIR)/*.cpp)
PROTO_SRC = $(wildcard $(SRC_DIR)/*.proto)
C_SRC = $(wildcard $(SRC_DIR)/*.c) $(patsubst $(SRC_DIR)/%.proto, $(SRC_DIR)/%.pb.c, $(PROTO_SRC))
CPP_OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(CPP_SRC))
C_OBJ =	$(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRC))

CPPFLAGS += -Inanopb -I$(ROOT_DIR)


.PHONY: all clean
all: main


$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CXX) $(CPPFLAGS) -MMD -c $< -o $@

main: $(CPP_OBJ) $(C_OBJ)
	$(CXX) $(CPPFLAGS) $(C_OBJ) $(CPP_OBJ) -o $@

clean:
	@find ./ -iregex '.*\.[od]$$' -exec rm {} +
	@find ./ -iregex '.*\.pb\.[ch]$$' -exec rm {} +	
	@rm -f main