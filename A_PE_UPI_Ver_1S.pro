r#-------------------------------------------------
#
# Project created by QtCreator 2018-06-19
#
#-------------------------------------------------

QT       += \
            core gui \
            serialport\
            network widgets\
            winextras

#DEFINES += QT_NO_WARNING_OUTPUT QT_NO_DEBUG_OUTPUT

DEFINES += SAMPLESPERPOINT=2048\ #keep samples point in power of 2 to keep the DMA notification aligned
        += BYTESPERSAMPLE=2\
        += NUMOFBANDS=3\
        += SPECTBASESIZE=400\
        += MAXVTWAMRANGES=10\
        += DAQ_DEBUG_LOGS\
        += ACTUALSYSTEM

#+= ACTUALSYSTEM
#+= DAQ_DEBUG_LOGS\
#+= SIG_FIFO_LEN=512\
#+= VERTCAL_RANGE=3.49\

CONFIG  += qwt

LIBS += vfw32.lib Gdi32.lib User32.lib $$PWD/Spectrum/c_header/spcm_win64_msvcpp.lib

#Spectrum Digitizer
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Spectrum/c_header/ -lspcm_win64_msvcpp
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Spectrum/c_header/ -lspcm_win64_msvcpp

INCLUDEPATH += $$PWD/Spectrum/c_header\
            += $$PWD/Spectrum/common \
            += $$PWD/lms \
            += $$PWD/Spectrum/sb5_file

DEPENDPATH  += $$PWD/c_header \
            += $$PWD/lms

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = A_PE_UPI_Ver_1S
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    laserController.cpp \
    ldvController.cpp \
    Spectrum/common/spcm_lib_card.cpp \
    Spectrum/common/spcm_lib_data.cpp \
    Spectrum/common/ostools/spcm_ostools_win.cpp \
    daqControllerS.cpp \
    dataProcessor.cpp \
    plot.cpp \
    spectrogram.cpp \
    dialogenlarge.cpp \
    lmsController.cpp \
    bandpasscontroller.cpp


HEADERS  += mainwindow.h \
    laserController.h \
    structDef.h \
    ldvController.h \
    daqControllerS.h \
    dataProcessor.h \
    plot.h \
    spectrogram.h \
    dialogenlarge.h \
    lmsController.h \
    bandpasscontroller.h

FORMS    += mainwindow.ui \
    dialogenlarge.ui

RESOURCES += \
    MyRes.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lms/ -lSCANalone4x64
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lms/ -lSCANalone4x64

