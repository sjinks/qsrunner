#ifndef MYAPPLICATION_H
#define MYAPPLICATION_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtScript/QScriptValue>

QT_FORWARD_DECLARE_CLASS(QCoreApplication)
QT_FORWARD_DECLARE_CLASS(QScriptContext)
QT_FORWARD_DECLARE_CLASS(QScriptEngine)
QT_FORWARD_DECLARE_CLASS(QScriptEngineDebugger)
QT_FORWARD_DECLARE_CLASS(QScriptProgram)

class MyApplication : public QObject {
	Q_OBJECT
public:
	MyApplication(QCoreApplication* app, bool gui);

	int exec(void);

private Q_SLOTS:
	void loadFile(const QString& name, bool once = true);
	void terminate(void);
	void signalHandlerException(const QScriptValue& exception);

private:
	bool m_gui;
	QCoreApplication* m_app;
	QScriptEngine* m_eng;
	QScriptEngineDebugger* m_dbg;

	static QMap<QString, QScriptProgram> loaded_files;

	static bool doLoadFile(const QString& name, QScriptEngine* eng, QScriptContext* ctx, bool once);

	static QScriptValue import(QScriptContext* context, QScriptEngine* engine);
	static QScriptValue availableExtensions(QScriptContext* context, QScriptEngine* engine);
	static QScriptValue print(QScriptContext* context, QScriptEngine* engine);
	static QScriptValue require(QScriptContext* context, QScriptEngine* engine);
	static QScriptValue requireOnce(QScriptContext* context, QScriptEngine* engine);
	static QScriptValue quit(QScriptContext* context, QScriptEngine* engine);
	static QScriptValue buildEnvironment(QScriptContext* context, QScriptEngine* engine);
};

#endif // MYAPPLICATION_H
