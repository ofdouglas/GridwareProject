include nanopb/extra/nanopb.mk

SRC_DIR := src
BUILD_DIR := build
PROJ_ROOT_DIR := $(CURDIR)

CPP_SRC = 	$(wildcard $(SRC_DIR)/*.cpp)
PROTO_SRC = $(wildcard $(SRC_DIR)/*.proto)
PROTO_C = 	$(patsubst $(SRC_DIR)/%.proto, $(SRC_DIR)/%.pb.c, $(PROTO_SRC))
PROTO_H = 	$(patsubst $(SRC_DIR)/%.proto, $(SRC_DIR)/%.pb.h, $(PROTO_SRC))
CPP_OBJ = 	$(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(CPP_SRC))
PROTO_OBJ =	$(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(PROTO_C))

CPPFLAGS += -Inanopb -I$(PROJ_ROOT_DIR)

.PHONY: all clean proto
all: main

DEPENDENCIES = $(PROTO_OBJ:.o=.d) $(CPP_OBJ:.o=.d)
-include $(DEPENDENCIES)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(PROTO_C) $(PROTO_H)
	$(CXX) $(CPPFLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.pb.o: $(SRC_DIR)/%.pb.c
	$(CXX) $(CPPFLAGS) -c $< -o $@

main: $(CPP_OBJ) $(PROTO_OBJ)
	$(CXX) $(CPPFLAGS) $(PROTO_OBJ) $(CPP_OBJ) -o $@

clean:
	@find ./ -iregex '.*\.[od]$$' -exec rm {} +
	@find ./ -iregex '.*\.pb\.[ch]$$' -exec rm {} +	
	@rm -f main