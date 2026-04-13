MAKEFILE = QtMakefile

TEMPLATE = app
TARGET = lanc_client
CONFIG += c++11 qt warn_on release
exists($${TARGET}.debug.pri) {
    include($${TARGET}.debug.pri) # Should contain: CONFIG += debug
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

