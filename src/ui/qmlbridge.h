#ifndef QMLBRIDGE_H
#define QMLBRIDGE_H

#include <QObject>
#include <QString>

#include "../control/transport.h"

/**
 * Class for communicating between QML and C++.
 * This is designed to be added as a QML type and used in QML.
 */
class QMLBridge : public QObject
{
	Q_OBJECT

	private:
		Transport *transport;

	public:
		explicit QMLBridge(QObject *parent = nullptr);

		Q_INVOKABLE QString getTransportState() const;
		Q_INVOKABLE void record();
		Q_INVOKABLE void stop();
		Q_INVOKABLE void play();
		Q_INVOKABLE void pause();

	signals:

		/**
		 * Signal emmitted when the Transport changes states.
		 * Keeps the UI's state machine on the same page.
		 */
		void stateChanged();
};

#endif // QMLBRIDGE_H