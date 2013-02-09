#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStringBuilder>
#include <QtCore/QStringList>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptProgram>
#include <QtScript/QScriptString>
#include <QtScript/QScriptValue>
#include <QtScriptTools/QScriptEngineDebugger>
#include <QApplication>
#include <cstdio>
#include "myapplication.h"

static int g_retcode = 0;
static MyApplication* g_inst = 0;
static QScriptString g_qs;
static QScriptString g_script;
static QScriptString g_system;
static QScriptString g_ap;
static QScriptString g_afp;
static QScriptString g_quit;
static QScriptString g_req;
static QScriptString g_ronce;

QMap<QString, QScriptProgram> MyApplication::loaded_files;

MyApplication::MyApplication(QCoreApplication* app, bool gui)
	: QObject(0), m_gui(gui), m_app(app), m_eng(new QScriptEngine(this)), m_dbg(0)
{
	g_inst = this;

	g_qs     = this->m_eng->toStringHandle(QLatin1String("qs"));
	g_script = this->m_eng->toStringHandle(QLatin1String("script"));
	g_system = this->m_eng->toStringHandle(QLatin1String("system"));
	g_ap     = this->m_eng->toStringHandle(QLatin1String("absolutePath"));
	g_afp    = this->m_eng->toStringHandle(QLatin1String("absoluteFilePath"));
	g_quit   = this->m_eng->toStringHandle(QLatin1String("quit"));
	g_req    = this->m_eng->toStringHandle(QLatin1String("require"));
	g_ronce  = this->m_eng->toStringHandle(QLatin1String("requireOnce"));

	QObject::connect(this->m_eng, SIGNAL(signalHandlerException(QScriptValue)), this, SLOT(signalHandlerException(QScriptValue)));
}

int MyApplication::exec(void)
{
	if (this->m_gui && qgetenv("QSRUNNER_NO_DEBUGGER").isEmpty()) {
		this->m_dbg = new QScriptEngineDebugger();
		this->m_dbg->attachTo(this->m_eng);
	}

	QScriptValue global = this->m_eng->globalObject();
	QScriptValue qs     = this->m_eng->newObject();
	QScriptValue script = this->m_eng->newObject();
	QScriptValue system = this->m_eng->newObject();

	global.setProperty(g_qs, qs);
	qs.setProperty(g_script, script);
	qs.setProperty(g_system, system);

	global.setProperty(QLatin1String("qApp"), this->m_eng->newQObject(this->m_app));

	global.setProperty(QLatin1String("print"), this->m_eng->newFunction(MyApplication::print));

	QScriptValue func_quit         = this->m_eng->newFunction(MyApplication::quit);
	QScriptValue func_require      = this->m_eng->newFunction(MyApplication::require);
	QScriptValue func_require_once = this->m_eng->newFunction(MyApplication::requireOnce);
	QScriptValue func_import       = this->m_eng->newFunction(MyApplication::import);

	script.setProperty(g_quit,  func_quit);
	script.setProperty(g_req,   func_require);
	script.setProperty(g_ronce, func_require_once);
	global.setProperty(g_quit,  func_quit);
	global.setProperty(g_req,   func_require);
	global.setProperty(g_ronce, func_require_once);

	script.setProperty(QLatin1String("extension"),           func_import);
	global.setProperty(QLatin1String("import"),              func_import);
	script.setProperty(QLatin1String("availableExtensions"), this->m_eng->newFunction(MyApplication::availableExtensions));

	QScriptValue buildenv = this->m_eng->newFunction(MyApplication::buildEnvironment);
	system.setProperty(QLatin1String("env"), buildenv, QScriptValue::ReadOnly | QScriptValue::Undeletable | QScriptValue::PropertyGetter);

	QStringList args = QCoreApplication::arguments();
	args.takeFirst();

	QString file = args.takeFirst();
	script.setProperty(QLatin1String("args"), this->m_eng->toScriptValue(args));

	QMetaObject::invokeMethod(this, "loadFile", Qt::QueuedConnection, Q_ARG(QString, file), Q_ARG(bool, false));
	this->m_gui ? QApplication::exec() : QCoreApplication::exec();
	return g_retcode;
}

void MyApplication::loadFile(const QString& name, bool once)
{
	if (!MyApplication::doLoadFile(name, this->m_eng, this->m_eng->currentContext(), once)) {
		qCritical("There was an error loading file %s", qPrintable(name));
		this->m_gui ? QApplication::exit(127) : QCoreApplication::exit(127);
	}
}

void MyApplication::terminate(void)
{
	this->m_gui ? QApplication::processEvents() : QCoreApplication::processEvents();
	QMetaObject::invokeMethod(g_inst->m_app, "quit", Qt::QueuedConnection);
}

void MyApplication::signalHandlerException(const QScriptValue& exception)
{
	qCritical("Uncaught exception from a signal handler: %s", qPrintable(exception.toString()));
}

QScriptValue MyApplication::buildEnvironment(QScriptContext* ctx, QScriptEngine* eng)
{
	Q_UNUSED(ctx)

	QMap<QString, QVariant> result;
	QStringList environment = QProcessEnvironment::systemEnvironment().toStringList();

	for (int i=0; i<environment.size(); ++i) {
		const QString& entry = environment.at(i);

		QStringList key_val = entry.split(QLatin1Char('='));
		if (1 == key_val.size()) {
			result.insert(key_val.at(0), QString());
		}
		else {
			result.insert(key_val.at(0), key_val.at(1));
		}
	}

	return eng->toScriptValue(result);
}

bool MyApplication::doLoadFile(const QString& name, QScriptEngine* eng, QScriptContext* ctx, bool once)
{
	QFileInfo info(name);

	QScriptValue script           = eng->globalObject().property(g_qs).property(g_script);
	QScriptValue oldFilePathValue = script.property(g_afp);
	QScriptValue oldPathValue     = script.property(g_ap);

	QString current = oldFilePathValue.toString();

	if (!current.isEmpty() && info.isRelative()) {
		QFileInfo current_info(current);

		info.setFile(current_info.path() % QLatin1Char('/') % info.filePath());
		Q_ASSERT(true == info.isAbsolute());
	}

	QString canonical = info.canonicalFilePath();
	bool known_file   = MyApplication::loaded_files.contains(canonical);
	if (once && known_file) {
		return true;
	}

	QString absolute_fname = info.absoluteFilePath();
	QString absolute_path  = info.absolutePath();

	QScriptProgram script_text;

	if (known_file) {
		script_text = MyApplication::loaded_files[canonical];
	}
	else {
		QFile file(canonical);
		if (file.open(QIODevice::ReadOnly)) {
			int lineno = 1;
			QString contents = QString::fromLocal8Bit(file.readAll().constData());
			file.close();

			if (contents.startsWith(QLatin1String("#!"))) {
				int pos = contents.indexOf(QLatin1Char('\n'));
				contents.remove(pos+1);
				++lineno;
			}

			script_text = QScriptProgram(contents, canonical, lineno);
			MyApplication::loaded_files.insert(canonical, script_text);
		}
		else {
			return false;
		}
	}

	script.setProperty(g_afp, eng->toScriptValue(absolute_fname));
	script.setProperty(g_ap,  eng->toScriptValue(absolute_path));

	if (ctx->parentContext()) {
		ctx->setActivationObject(ctx->parentContext()->activationObject());
		ctx->setThisObject(ctx->parentContext()->thisObject());
	}

	QScriptValue res = eng->evaluate(script_text);

	script.setProperty(g_afp, oldFilePathValue);
	script.setProperty(g_ap,  oldPathValue);

	if (eng->hasUncaughtException()) {
		QStringList backtrace = eng->uncaughtExceptionBacktrace();
		qWarning("%s", qPrintable(QString::fromLatin1("\t%1\n%2\n\n").arg(res.toString()).arg(backtrace.join(QLatin1String("\n")))));
		eng->abortEvaluation(1);
		qApp->exit(255);
	}

	return true;
}

QScriptValue MyApplication::import(QScriptContext* context, QScriptEngine* engine)
{
	return engine->importExtension(context->argument(0).toString());
}

QScriptValue MyApplication::availableExtensions(QScriptContext*, QScriptEngine* engine)
{
	return engine->toScriptValue(engine->availableExtensions());
}

QScriptValue MyApplication::print(QScriptContext* context, QScriptEngine* engine)
{
	QString result;
	bool starts_with_space = false;
	bool ends_with_space   = true;
	for (int i=0; i<context->argumentCount(); ++i) {
		QString arg = context->argument(i).toString();
		if (!arg.isEmpty()) {
			starts_with_space = arg[0].isSpace();
			if (!ends_with_space && !starts_with_space) {
				result.append(QLatin1Char(' '));
			}

			ends_with_space = arg[arg.length()-1].isSpace();
		}

		result.append(context->argument(i).toString());
	}

	std::fprintf(stdout, "%s", qPrintable(result));
	std::fflush(stdout);
	return engine->undefinedValue();
}

QScriptValue MyApplication::require(QScriptContext* context, QScriptEngine* engine)
{
	QString f(context->argument(0).toString());
	if (!MyApplication::doLoadFile(f, engine, context, false)) {
		return context->throwError(QString::fromLatin1("Failed to resolve include: %1").arg(f));
	}

	return engine->toScriptValue(true);
}

QScriptValue MyApplication::requireOnce(QScriptContext* context, QScriptEngine* engine)
{
	QString f(context->argument(0).toString());
	if (!MyApplication::doLoadFile(f, engine, context, true)) {
		return context->throwError(QString::fromLatin1("Failed to resolve include: %1").arg(f));
	}

	return engine->toScriptValue(true);
}

QScriptValue MyApplication::quit(QScriptContext* context, QScriptEngine* engine)
{
	QScriptValue rc(context->argumentCount() > 0 ? context->argument(0).toUInt32() : 0);
	g_retcode = rc.toUInt32();

	if (g_inst->m_dbg) {
		g_inst->m_dbg->detach();
		g_inst->m_dbg->deleteLater();
		g_inst->m_dbg = 0;
	}

	engine->disconnect();
	engine->abortEvaluation(rc);

	QMetaObject::invokeMethod(g_inst, "terminate", Qt::QueuedConnection);
	return rc;
}
