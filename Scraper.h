#pragma once

#include <QObject>
#include <QFile>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include <qgumbodocument.h>
#include <qgumbonode.h>

#define PURCHASE_HISTORY_URL "https://www.ebay.com/bin/purchaseHistory?item="

const QString CSV_SEPARATOR = "\t";

class Scraper : public QObject
{
	Q_OBJECT
public:
	explicit Scraper(QObject* parent = nullptr, const QString& dataFilePath = "");

	void scrapePage(const QString& html);

private:
	QString _filePath;

	int _resultCount = 0;

	struct purchaseHistoryResult
	{
		QString id;
		QString name;
		QString price;
		int quantity;
		QString date;
	};;
	QList<purchaseHistoryResult> _purchaseResults;

	bool _nextButtonDisabled = false;
	QString _nextPageUrl;

signals:
	void log(const QString& text);
	void updateProgress(const int resultsDone);
	void requestNextPage(const QString& url, int prevResults);
	void finished();

private slots:
	void processPurchaseHistoryPage(const QString& html, QWebEnginePage* page, const QString& id);
};

