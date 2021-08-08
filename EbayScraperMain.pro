QT       += core gui webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SUBDIRS += libs/QGumboParser/QGumboParser

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Cleanup.cpp \
    Scraper.cpp \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    Cleanup.h \
    MainWindow.h \
    Scraper.h

FORMS += \
    Cleanup.ui \
    MainWindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/libs/QGumboParser/QGumboParser/release/ -lQGumboParser
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/libs/QGumboParser/QGumboParser/debug/ -lQGumboParser
else:unix: LIBS += -L$$OUT_PWD/libs/QGumboParser/QGumboParser/ -lQGumboParser

INCLUDEPATH += $$PWD/libs/QGumboParser/QGumboParser
DEPENDPATH += $$PWD/libs/QGumboParser/QGumboParser

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/libs/QGumboParser/QGumboParser/release/libQGumboParser.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/libs/QGumboParser/QGumboParser/debug/libQGumboParser.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/libs/QGumboParser/QGumboParser/release/QGumboParser.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/libs/QGumboParser/QGumboParser/debug/QGumboParser.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/libs/QGumboParser/QGumboParser/libQGumboParser.a
