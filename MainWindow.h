#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QFileDialog>
#include <QMessageBox>

#include <qgumbodocument.h>
#include <qgumbonode.h>

#include "Scraper.h"
#include "Cleanup.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private:
	Ui::MainWindow *_ui;

	QWebEngineView* _webEngineView = nullptr;

	Scraper* _scraper = nullptr;

	QString _searchPageHtml;
	bool _scrapeStarted = false;
	QString _dataFilePath;

	int _resultCount = 0;
	int _totalResultsDone = 0;

	bool _terminateRequested = false;

private slots:
	void loadUrl();
	void onPageLoadStarted();
	void onPageLoadFinished(bool ok);
	void onParsePageButtonPressed();
	void testParsePage(const QString& html);

	void selectFilePath();
	void startScrape();
	void logToConsole(const QString& text);
	void updateProgress(const int resultsDoneOnCurrentPage);
	void onNextPageRequested(const QString& url, int prevResults);
	void onTerminateRequested();
	void onScraperFinished();

	void showCleanupWindow();
};
