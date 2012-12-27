QT += script scripttools

greaterThan(QT_MAJOR_VERSION, 4) {
	QT += widgets
}

TARGET   = qsrunner
CONFIG  += console release
CONFIG  -= app_bundle
TEMPLATE = app
VERSION  = 2.0.0

SOURCES += \
	main.cpp \
	myapplication.cpp

HEADERS += myapplication.h
