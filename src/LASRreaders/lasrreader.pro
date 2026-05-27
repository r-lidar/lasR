TEMPLATE = lib
CONFIG += shared

QMAKE_CXXFLAGS += -std=c++20
QMAKE_CXXFLAGS += -Wno-unused-parameter -Wno-implicit-fallthrough
QMAKE_CXXFLAGS += -O2

DESTDIR     = $$OUT_PWD/liblasrreader
OBJECTS_DIR = $$OUT_PWD/liblasrreader/.obj
MOC_DIR     = $$OUT_PWD/liblasrreader/.moc

DEFINES += NOGDAL

SOURCES += $$files(*.cpp)
SOURCES -= $$files(EPTio.cpp)

INCLUDEPATH += \
    ../vendor/LASlib \
    ../vendor/LASzip
