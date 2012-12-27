#include <QtCore/QScopedPointer>
#include <QApplication>
#include "myapplication.h"

int main(int argc, char** argv)
{
	if (argc < 2) {
		return 1;
	}

	bool need_gui = qgetenv("QSRUNNER_NO_GUI").isEmpty() && !qgetenv("DISPLAY").isEmpty();

	QScopedPointer<QCoreApplication> app(need_gui ? new QApplication(argc, argv) : new QCoreApplication(argc, argv));

	MyApplication m(app.data(), need_gui);
	return m.exec();
}
