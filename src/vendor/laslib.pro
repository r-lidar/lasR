TEMPLATE = lib
CONFIG += shared

QMAKE_CXXFLAGS += -std=c++20
QMAKE_CXXFLAGS += -Wno-unused-parameter -Wno-implicit-fallthrough
QMAKE_CXXFLAGS += -O2

DESTDIR     = $$OUT_PWD/liblaslib
OBJECTS_DIR = $$OUT_PWD/liblaslib/.obj
MOC_DIR     = $$OUT_PWD/liblaslib/.moc

DEFINES += NOGDAL

SOURCES += \
    $$files(LASlib/*.cpp) \
    $$files(LASzip/*.cpp)

INCLUDEPATH += \
    LASlib \
    LASzip
