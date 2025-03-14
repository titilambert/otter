/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2014 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "Application.h"
#include "ActionsManager.h"
#include "BookmarksManager.h"
#include "Console.h"
#include "HistoryManager.h"
#include "NetworkManagerFactory.h"
#include "SearchesManager.h"
#include "SettingsManager.h"
#include "TransfersManager.h"
#include "WebBackendsManager.h"
#include "./config.h"
#include "../ui/MainWindow.h"

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLocale>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTranslator>
#include <QtWidgets/QMessageBox>
#include <QtNetwork/QLocalSocket>

namespace Otter
{

Application* Application::m_instance = NULL;

Application::Application(int &argc, char **argv) : QApplication(argc, argv),
	m_localServer(NULL)
{
	setApplicationName(QLatin1String("Otter"));
	setApplicationVersion(QLatin1String("0.9.02-dev"));
	setWindowIcon(QIcon::fromTheme(QLatin1String("otter-browser"), QIcon(QLatin1String(":/icons/otter-browser.png"))));

	m_instance = this;

	QString profilePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QLatin1String("/otter");
	QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
	QCommandLineParser *parser = getParser();
	parser->process(arguments());

	const bool isPortable = parser->isSet(QLatin1String("portable"));

	if (isPortable)
	{
		profilePath = applicationDirPath() + QLatin1String("/profile");
		cachePath = applicationDirPath() + QLatin1String("/cache");
	}

	if (parser->isSet(QLatin1String("profile")))
	{
		profilePath = parser->value(QLatin1String("profile"));

		if (QFileInfo(profilePath).isRelative())
		{
			profilePath = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QLatin1String("/otter/profiles/")).absoluteFilePath(profilePath);
		}
	}

	profilePath = QFileInfo(profilePath).absoluteFilePath();

	if (parser->isSet(QLatin1String("cache")))
	{
		cachePath = parser->value(QLatin1String("cache"));
	}

	cachePath = QFileInfo(cachePath).absoluteFilePath();

	delete parser;

	QCryptographicHash hash(QCryptographicHash::Md5);
	hash.addData(profilePath.toUtf8());

	const QString identifier(hash.result().toHex());
	const QString server = applicationName() + (identifier.isEmpty() ? QString() : (QLatin1Char('-') + identifier));
	QLocalSocket socket;
	socket.connectToServer(server);

	if (socket.waitForConnected(500))
	{
		const QStringList decodedArguments = arguments();
		QStringList encodedArguments;

		for (int i = 0; i < decodedArguments.count(); ++i)
		{
			encodedArguments.append(decodedArguments.at(i).toUtf8().toBase64());
		}

		QTextStream stream(&socket);
		stream << encodedArguments.join(QLatin1Char(' ')).toUtf8().toBase64();
		stream.flush();

		socket.waitForBytesWritten();

		return;
	}

	m_localServer = new QLocalServer(this);

	connect(m_localServer, SIGNAL(newConnection()), this, SLOT(newConnection()));

	if (!m_localServer->listen(server) && m_localServer->serverError() == QAbstractSocket::AddressInUseError && QFile::exists(m_localServer->fullServerName()))
	{
		QFile::remove(m_localServer->fullServerName());

		m_localServer->listen(server);
	}

	if (!QFile::exists(profilePath))
	{
		QDir().mkpath(profilePath);
	}

	Console::createInstance(this);

	SettingsManager::createInstance(profilePath + QLatin1String("/otter.conf"), this);

	QSettings defaults(QLatin1String(":/schemas/options.ini"), QSettings::IniFormat, this);
	const QStringList groups = defaults.childGroups();

	for (int i = 0; i < groups.count(); ++i)
	{
		defaults.beginGroup(groups.at(i));

		const QStringList keys = defaults.childGroups();

		for (int j = 0; j < keys.count(); ++j)
		{
			SettingsManager::setDefaultValue(QStringLiteral("%1/%2").arg(groups.at(i)).arg(keys.at(j)), defaults.value(QStringLiteral("%1/value").arg(keys.at(j))));
		}

		defaults.endGroup();
	}

	SettingsManager::setDefaultValue(QLatin1String("Paths/Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
	SettingsManager::setDefaultValue(QLatin1String("Paths/SaveFile"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));

	SessionsManager::createInstance(profilePath, cachePath, this);

	ActionsManager::createInstance(this);

	NetworkManagerFactory::createInstance(this);

	BookmarksManager::createInstance(this);

	HistoryManager::createInstance(this);

	WebBackendsManager::createInstance(this);

	SearchesManager::createInstance(this);

	TransfersManager::createInstance(this);

	QTranslator *qtTranslator = new QTranslator(this);
	qtTranslator->load(QLatin1String("qt_") + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));

	QString localePath = INSTALL_PREFIX + QLatin1String("/share/otter-browser/locale/");

	if (isPortable || QFile::exists(applicationDirPath() + QLatin1String("/locale/")))
	{
		localePath = applicationDirPath() + QLatin1String("/locale/");
	}

	QTranslator *applicationTranslator = new QTranslator(this);
	applicationTranslator->load(QLatin1String("otter-browser_") + QLocale::system().name(), localePath);

	installTranslator(qtTranslator);
	installTranslator(applicationTranslator);
	setQuitOnLastWindowClosed(true);
}

Application::~Application()
{
	for (int i = 0; i < m_windows.size(); ++i)
	{
		m_windows.at(i)->deleteLater();
	}
}

void Application::removeWindow(MainWindow *window)
{
	m_windows.removeAll(window);

	window->deleteLater();
}

void Application::newConnection()
{
	QLocalSocket *socket = m_localServer->nextPendingConnection();

	if (!socket)
	{
		return;
	}

	socket->waitForReadyRead(1000);

	MainWindow *window = (getWindows().isEmpty() ? NULL : getWindow());
	QString data;
	QTextStream stream(socket);
	stream >> data;

	const QStringList encodedArguments = QString(QByteArray::fromBase64(data.toUtf8())).split(QLatin1Char(' '));
	QStringList decodedArguments;

	for (int i = 0; i < encodedArguments.count(); ++i)
	{
		decodedArguments.append(QString(QByteArray::fromBase64(encodedArguments.at(i).toUtf8())));
	}

	QCommandLineParser *parser = getParser();
	parser->parse(decodedArguments);

	const QString session = parser->value(QLatin1String("session"));
	const bool privateSession = parser->isSet(QLatin1String("privatesession"));

	if (session.isEmpty())
	{
		if (!window || !SettingsManager::getValue(QLatin1String("Browser/OpenLinksInNewTab")).toBool() || (privateSession && !window->getWindowsManager()->isPrivate()))
		{
			window = createWindow(privateSession);
		}
	}
	else
	{
		const SessionInformation sessionData = SessionsManager::getSession(session);

		if (sessionData.clean || QMessageBox::warning(NULL, tr("Warning"), tr("This session was not saved correctly.\nAre you sure that you want to restore this session anyway?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
		{
			for (int i = 0; i < sessionData.windows.count(); ++i)
			{
				createWindow(privateSession, false, sessionData.windows.at(i));
			}
		}
	}

	if (window)
	{
		if (!parser->positionalArguments().isEmpty())
		{
			const QStringList urls = parser->positionalArguments();

			for (int i = 0; i < urls.count(); ++i)
			{
				window->openUrl(QUrl(urls.at(i)));
			}
		}
		else if (session.isEmpty())
		{
			window->openUrl();
		}
	}

	delete socket;

	if (window)
	{
		window->raise();
		window->activateWindow();
	}

	delete parser;
}

void Application::newWindow(bool privateSession, bool background, const QUrl &url)
{
	MainWindow *window = createWindow(privateSession, background);

	if (url.isValid() && window)
	{
		window->openUrl(url);
	}
}

Application* Application::getInstance()
{
	return m_instance;
}

MainWindow* Application::createWindow(bool privateSession, bool background, const SessionMainWindow &windows)
{
	MainWindow *window = new MainWindow(privateSession, windows);

	m_windows.prepend(window);

	if (background)
	{
		window->setAttribute(Qt::WA_ShowWithoutActivating, true);
	}

	window->show();

	if (background)
	{
		window->lower();
		window->setAttribute(Qt::WA_ShowWithoutActivating, false);
	}

	connect(window, SIGNAL(requestedNewWindow(bool,bool,QUrl)), this, SLOT(newWindow(bool,bool,QUrl)));

	return window;
}

MainWindow* Application::getWindow()
{
	if (m_windows.isEmpty())
	{
		return createWindow();
	}

	return m_windows[0];
}

QCommandLineParser* Application::getParser() const
{
	QCommandLineParser *parser = new QCommandLineParser();
	parser->addHelpOption();
	parser->addVersionOption();
	parser->addPositionalArgument("url", QCoreApplication::translate("main", "URL to open"), QLatin1String("[url]"));
	parser->addOption(QCommandLineOption(QLatin1String("cache"), QCoreApplication::translate("main", "Uses <path> as cache directory"), QLatin1String("path"), QString()));
	parser->addOption(QCommandLineOption(QLatin1String("profile"), QCoreApplication::translate("main", "Uses <path> as profile directory"), QLatin1String("path"), QString()));
	parser->addOption(QCommandLineOption(QLatin1String("session"), QCoreApplication::translate("main", "Restores session <session> if it exists"), QLatin1String("session"), QString()));
	parser->addOption(QCommandLineOption(QLatin1String("privatesession"), QCoreApplication::translate("main", "Starts private session")));
	parser->addOption(QCommandLineOption(QLatin1String("portable"), QCoreApplication::translate("main", "Sets profile and cache paths to directories inside the same directory as that of application binary")));

	return parser;
}

QList<MainWindow*> Application::getWindows()
{
	return m_windows;
}

bool Application::isRunning() const
{
	return (m_localServer == NULL);
}

}
