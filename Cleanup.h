#pragma once

#include <QObject>
#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>

const QString OUT_CSV_SEPARATOR = "\t";

const QLocale EN_US_LOCALE(QLocale::English, QLocale::UnitedStates);

namespace Ui
{
	class Cleanup;
}

class Cleanup : public QWidget
{
	Q_OBJECT
public:
	explicit Cleanup(QWidget* parent = nullptr);
	~Cleanup();

private:
	Ui::Cleanup* _ui = nullptr;

	bool ebayPriceToISO(QString ebayPrice, QString& outISOCode, QString& outPrice) const;
	QUrl getCurrencyConverterUrl(const QString& targetISOCode, const QString& amount, const QDate& date) const;

	QNetworkAccessManager* _networkAccessManager = nullptr;

	bool _cleanupRunning = false;
	bool _requestTerminate = false;
	int _lineCount = 0;

signals:
	void cleanupFinished();

private slots:
	void selectSourceFile();
	void selectDestFile();
	void onCancelButtonPressed();

	void doCleanup();
	void updateProgress(const int currentLine);
	void onCleanupFinished();
};

