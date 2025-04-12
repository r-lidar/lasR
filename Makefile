CXX = g++
CXXFLAGS = -Wall -Wno-unused-parameter -std=c++17 -O2 -fopenmp
PICFLAGS = -fPIC

SRC_DIR = ./src
BIN_DIR = ./bin

STATIC_LIB = $(BIN_DIR)/liblasr.a
SHARED_LIB = $(BIN_DIR)/liblasr.so

ifeq ($(OS),Windows_NT)
    DETECT_OS = Windows

    RTOOLS = C:/rtools44/x86_64-w64-mingw32.static.posix
    MKDIR = mkdir
    RM = del

    RWININCLUDE = -I$(RTOOLS)/include
    RWINLIBS = -L$(RTOOLS)/lib

    LIBS = $(RWINLIBS) -lgdal -larmadillo -lopenblas -lgomp -lmingwthrd -lgfortran -lquadmath -lpoppler -lharfbuzz -lfreetype -lharfbuzz_too -lfreetype_too -lintl -lwinmm -lole32 -lshlwapi -luuid -lpng -lgif -lnetcdf -lhdf5_hl -lblosc -llz4 -lgta -lmfhdf -lportablexdr -ldf -lkea -lhdf5_cpp -lhdf5 -lwsock32 -lsz -lopenjp2 -llcms2 -lpng16 -lpcre2-8 -lspatialite -lidn2 -lunistring -lcharset -lssh2 -lgcrypt -lgpg-error -ladvapi32 -lwldap32 -ldl -lmysqlclient -lpq -lpgcommon -lpgport -lpthread -lshell32 -lsecur32 -lodbc32 -lodbccp32 -lfreexl -liconv -lminizip -lbz2 -lbcrypt -lssl -lcrypto -lws2_32 -lgdi32 -lcrypt32 -lexpat -lxml2 -lgeos -lpsapi -lsqlite3 -lwebp -lsharpyuv -lm -lzstd -lz -lcurl -ljson-c -lstdc++ -lidn2 -lunistring -liconv -lcharset -lssh2 -lssh2 -lgcrypt -lgpg-error -lws2_32 -lgcrypt -lgpg-error -lws2_32 -lgcrypt -lgpg-error -lws2_32 -lz -lbcrypt -ladvapi32 -lcrypt32 -lssl -lcrypto -lssl -lz -lws2_32 -lgdi32 -lcrypt32 -lcrypto -lz -lws2_32 -lgdi32 -lcrypt32 -lgdi32 -lwldap32 -lzstd -lz -lws2_32 -lpthread -lssh2 -lgcrypt -lgpg-error -lws2_32 -lgcrypt -lgpg-error -lws2_32 -lws2_32 -lz -lcrypt32 -lssl -lcrypto -lz -lws2_32 -lgdi32 -lcrypt32 -lz -lws2_32 -lgdi32 -lz -lgeos_c -lgeos -lstdc++ -lm -lproj -lstdc++ -lsqlite3 -ldl -ltiff -lwebp -lm -lsharpyuv -lm -llzma -ljpeg -lcurl -lidn2 -lunistring -liconv -lcharset -lssh2 -lgcrypt -lgpg-error -lbcrypt -ladvapi32 -lssl -lcrypto -lcrypt32 -lgdi32 -lwldap32 -lzstd -lz -lws2_32 -lpthread -lidn2 -lunistring -liconv -lcharset -lssh2 -lssh2 -lgcrypt -lgpg-error -lws2_32 -lgcrypt -lgpg-error -lws2_32 -lgcrypt -lgpg-error -lws2_32 -lz -lbcrypt -ladvapi32 -lcrypt32 -lssl -lcrypto -lssl -lz -lws2_32 -lgdi32 -lcrypt32 -lcrypto -lz -lws2_32 -lgdi32 -lcrypt32 -lgdi32 -lwldap32 -lzstd -lz -lws2_32 -lpthread -lglib-2.0 -lgeotiff -lpsl -ldeflate -llerc -lbrotlidec -lbrotlienc -lbrotlicommon -lws2_32 -lunistring -fopenmp
    INCLUDES = -I./$(SRC_DIR)/ -I./$(SRC_DIR)/LASRcore/ -I./$(SRC_DIR)/LASRstages/ -I./$(SRC_DIR)/LASRreaders/ -I./$(SRC_DIR)/vendor/ -I./$(SRC_DIR)/vendor/LASlib/ -I./$(SRC_DIR)/vendor/LASzip/ $(RWININCLUDE)
else
    DETECT_OS = Linux

    MKDIR = mkdir -p
    RM = rm -f

    GDAL_INCLUDE = $(shell gdal-config --cflags)
    GDAL_LIBS = $(shell gdal-config --libs)
    PROJ_INCLUDE = $(shell pkg-config --cflags proj)
    PROJ_LIBS = $(shell pkg-config --libs proj)
    BLAS_INCLUDE = $(shell pkg-config --cflags blas)
    BLAS_LIBS = $(shell pkg-config --libs blas)
    LAPACK_LIBS = $(shell pkg-config --libs lapack)

    LIBS = $(GDAL_LIBS) $(PROJ_LIBS)
    INCLUDES = -I./$(SRC_DIR)/ -I./$(SRC_DIR)/LASRcore/ -I./$(SRC_DIR)/LASRstages/ -I./$(SRC_DIR)/LASRreaders/ -I./$(SRC_DIR)/vendor/ -I./$(SRC_DIR)/vendor/LASlib/ -I./$(SRC_DIR)/vendor/LASzip/ $(GDAL_INCLUDE) $(PROJ_INCLUDE) $(BLAS_INCLUDE)
endif

TARGET = $(BIN_DIR)/lasr
SRCS = $(wildcard $(SRC_DIR)/LASRcore/*.cpp $(SRC_DIR)/LASRstages/*.cpp  $(SRC_DIR)/LASRreaders/*.cpp  $(SRC_DIR)/vendor/*/*.cpp $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BIN_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(MKDIR) $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

bin/main.o: src/main.cpp
	$(MKDIR) $(dir $@)
	$(CXX) -DEXECUTABLE $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(MKDIR) $(dir $@)
	$(CXX) $(if $(PICCXXFLAGS),$(PICCXXFLAGS),$(CXXFLAGS)) $(INCLUDES) -c $< -o $@

libstatic: $(STATIC_LIB)

$(STATIC_LIB): $(OBJS)
	@echo "Creating static library: $@"
	$(MKDIR) $(BIN_DIR)
	ar rcs $@ $^

libshared: PICCXXFLAGS := $(CXXFLAGS) $(PICFLAGS)
libshared: $(SHARED_LIB)

$(SHARED_LIB): $(OBJS)
	@echo "Creating shared library: $@"
	$(MKDIR) $(BIN_DIR)
ifeq ($(DETECT_OS),Windows)
	$(CXX) -shared -o $@ $^ $(LIBS)
else
	$(CXX) -shared -o $@ $^ $(LIBS)
endif

clean:
	$(RM) $(OBJS) $(TARGET) $(STATIC_LIB) $(SHARED_LIB)

.PHONY: all clean libstatic libshared

