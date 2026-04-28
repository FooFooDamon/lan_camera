MAKEFILE = QtMakefile

TEMPLATE = app
TARGET = lanc_client
CONFIG += c++11 qt warn_on release
exists($${TARGET}.debug.pri) {
    include($${TARGET}.debug.pri) # Should contain: CONFIG += debug
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS_RELEASE += -g
    QMAKE_CFLAGS_RELEASE += -g
    !isEmpty(QMAKE_POST_LINK) {
        QMAKE_POST_LINK += &&
    }
    QMAKE_POST_LINK += objcopy --only-keep-debug $${TARGET} $${TARGET}.dbgi
    QMAKE_POST_LINK += && $${QMAKE_STRIP} --strip-all $${TARGET}
    QMAKE_POST_LINK += && objcopy --add-gnu-debuglink=$${TARGET}.dbgi $${TARGET}
}

FORMS += *.ui
HEADERS += Q*.hpp
SOURCES += *.c *.cpp
# Preparations for multimedia usage on Debian and its derived systems:
#   sudo apt install qtmultimedia5-dev # For compilation
#   sudo apt install libqt5multimedia5-plugins # Solve the error: no service found for - "org.qt-project.qt.mediaplayer"
#   sudo apt install gstreamer1.0-libav # Install necessary decoders
#   sudo apt install gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly # GStreamer backend
#   sudo apt install gstreamer1.0-plugins-bad # Optional
QT += widgets multimedia multimediawidgets
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += openglwidgets
}

