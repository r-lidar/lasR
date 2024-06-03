CXX = g++
CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -std=c++17 -O2 -fopenmp

SRC_DIR = src
BIN_DIR = bin

GDAL_INCLUDE = $(shell gdal-config --cflags)
GDAL_LIBS = $(shell gdal-config --libs)
PROJ_INCLUDE = $(shell pkg-config --cflags proj)
PROJ_LIBS = $(shell pkg-config --libs proj)
BLAS_INCLUDE = $(shell pkg-config --cflags blas)
BLAS_LIBS = $(shell pkg-config --libs blas)
LAPACK_LIBS = $(shell pkg-config --libs lapack)

TARGET = $(BIN_DIR)/lasr
INCLUDES = -I./$(SRC_DIR)/ -I./$(SRC_DIR)/LASRcore/ -I./$(SRC_DIR)/LASRstages/ -I./$(SRC_DIR)/vendor/ -I./$(SRC_DIR)/vendor/LASlib/ -I./$(SRC_DIR)/vendor/LASzip/  $(GDAL_INCLUDE)  $(PROJ_INCLUDE) $(BLAS_INCLUDE)
SRCS = $(wildcard $(SRC_DIR)/LASRcore/*.cpp $(SRC_DIR)/LASRstages/*.cpp $(SRC_DIR)/vendor/*/*.cpp $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BIN_DIR)/%.o,$(SRCS))



# Default target
all: $(TARGET)

# Rule to link the target binary
$(TARGET): $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(GDAL_LIBS) $(PROJ_LIBS) $(BLAS_LIBS) $(LAPACK_LIBS)

# Rule to compile the source files into object files
$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean rule to remove generated files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
