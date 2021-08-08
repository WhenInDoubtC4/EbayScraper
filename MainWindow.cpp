#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDebug>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
  , _ui(new Ui::MainWindow)
  , _webEngineView(new QWebEngineView(this))
{
	_ui->setupUi(this);

	_ui->rootVerticalLayout->addWidget(_webEngineView);

	//Set browser:sidebar default ratio to 4:1
	_ui->splitter->setStretchFactor(0, 4);
	_ui->splitter->setStretchFactor(1, 1);

	_webEngineView->page()->profile()->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

	QObject::connect(_ui->urlGoButton, &QPushButton::pressed, this, &MainWindow::loadUrl);
	QObject::connect(_webEngineView->page(), &QWebEnginePage::loadStarted, this, &MainWindow::onPageLoadStarted);
	QObject::connect(_webEngineView->page(), &QWebEnginePage::loadFinished, this, &MainWindow::onPageLoadFinished);
	QObject::connect(_ui->parsePageButton, &QPushButton::pressed, this, &MainWindow::onParsePageButtonPressed);

	QObject::connect(_ui->dataFilePathButton, &QPushButton::pressed, this, &MainWindow::selectFilePath);
	QObject::connect(_ui->startButton, &QPushButton::pressed, this, &MainWindow::startScrape);
	QObject::connect(_ui->terminateButton, &QPushButton::pressed, this, &MainWindow::onTerminateRequested);

	QObject::connect(_ui->action_cleanup, &QAction::triggered, this, &MainWindow::showCleanupWindow);
}

MainWindow::~MainWindow()
{
	delete _webEngineView;

	delete _ui;
}

void MainWindow::loadUrl()
{
	_ui->statusbar->showMessage("Loading...");

	_ui->console->appendPlainText("Loading page...");

	//Clear parse data
	_ui->statusLineEdit->setText("Unknown");
	_ui->statusLineEdit->setStyleSheet("background-color: gray;");
	_ui->resultCountLineEdit->setText("0");
	_ui->startButton->setEnabled(false);

	_webEngineView->load(QUrl(_ui->urlLineEdit->text()));
}

void MainWindow::onPageLoadStarted()
{
	if (!_scrapeStarted)
	{
		_ui->parsePageButton->setEnabled(false);
	}
}

void MainWindow::onPageLoadFinished([[maybe_unused]] bool ok)
{
	_ui->statusbar->showMessage("Done");

	if (!_scrapeStarted)
	{
		_ui->console->appendPlainText("Load finished");
		_ui->parsePageButton->setEnabled(true);
	}
	else
	{
		_webEngineView->page()->toHtml([=](const QString& html)
		{
			_scraper->scrapePage(html);
		});

		_ui->console->appendPlainText("Next page loaded");
	}
}

void MainWindow::onParsePageButtonPressed()
{
	_ui->console->appendPlainText("Parsing HTML...");
	_webEngineView->page()->toHtml(std::bind(&MainWindow::testParsePage, this, std::placeholders::_1));
}

void MainWindow::testParsePage(const QString& html)
{
	try
	{
		auto htmlDoc = QGumboDocument::parse(html);
		QGumboNode rootNode = htmlDoc.rootNode();

		QGumboNodes soldCountNodes = rootNode.getElementsByClassName("rcnt");
		if(soldCountNodes.empty()) throw new std::runtime_error("Malformed page");
		_ui->resultCountLineEdit->setText(soldCountNodes[0].innerText());

		_resultCount = soldCountNodes[0].innerText().replace(",", "").toInt();

		QGumboNodes resultNodes = rootNode.getElementById("ListViewInner")[0].childNodes();
		for (QGumboNode resultRootNode : resultNodes)
		{
			QGumboNodes titleRootNodes = resultRootNode.getElementsByClassName("lvtitle");
			if (titleRootNodes.empty()) continue;

			const QString id = resultRootNode.getAttribute("listingid");

			QGumboNode titleLinkNode = titleRootNodes[0].getElementsByTagName(HtmlTag::A)[0];
			_ui->console->appendPlainText("- Title: " + titleLinkNode.innerText() + " | ID: " + id);
		}

		QGumboNode prevButtonNode = rootNode.getElementsByClassName("pagn-prev")[0].getElementsByTagName(HtmlTag::A)[0];
		const bool prevButtonDisabled = prevButtonNode.hasAttribute("aria-disabled");

		_ui->statusLineEdit->setText("OK");
		_ui->statusLineEdit->setStyleSheet("background-color: green;");
		_ui->console->appendPlainText("Parsing done.");
		_ui->startButton->setEnabled(true);

		if (!prevButtonDisabled)
		{
			_ui->statusLineEdit->setText("Warning: Not on first page");
			_ui->statusLineEdit->setStyleSheet("background-color: yellow;");
			_ui->console->appendPlainText("Prev button is not disabled, search not on first page?");
		}
	}
	catch (...)
	{
		_ui->statusLineEdit->setText("Error");
		_ui->statusLineEdit->setStyleSheet("background-color: red;");
		_ui->console->appendPlainText("Parse ended with error");
		_ui->startButton->setEnabled(false);
	}
}

void MainWindow::selectFilePath()
{
	//TODO: File path not read from line edit
	_dataFilePath = QFileDialog::getSaveFileName(this, "Save data file", "dataOut.csv", "CSV files (*.csv);;All files (*.*)");
	_ui->dataFilePathLineEdit->setText(_dataFilePath);
}

void MainWindow::startScrape()
{
	//Check file path
	if (_dataFilePath.isEmpty())
	{
		QMessageBox::critical(this, "Error", "No data file path selected");
		return;
	}

	_scraper = new Scraper(this, _dataFilePath);
	QObject::connect(_scraper, &Scraper::log, this, &MainWindow::logToConsole);
	QObject::connect(_scraper, &Scraper::updateProgress, this, &MainWindow::updateProgress);
	QObject::connect(_scraper, &Scraper::requestNextPage, this, &MainWindow::onNextPageRequested);
	QObject::connect(_scraper, &Scraper::finished, this, &MainWindow::onScraperFinished);

	_webEngineView->page()->toHtml([=](const QString& html)
	{
		_scraper->scrapePage(html);
	});

	_terminateRequested = false;
	_scrapeStarted = true;
	_ui->progressBar->setEnabled(true);
	_ui->startButton->setEnabled(false);
	_ui->terminateButton->setEnabled(true);
	_ui->parsePageButton->setEnabled(false);
}

void MainWindow::logToConsole(const QString& text)
{
	_ui->console->appendPlainText(text);
}

void MainWindow::updateProgress(const int resultsDoneOnCurrentPage)
{
	const int totalDone = resultsDoneOnCurrentPage + _totalResultsDone;
	const float progress = float(totalDone) / float(_resultCount);
	_ui->progressBar->setValue(qRound(progress * 100.f));
	_ui->progressBar->setFormat(QString::number(progress * 100.f, 'f', 2)  + "% (" + QString::number(totalDone) + "/" + QString::number(_resultCount) + ")");
}

void MainWindow::onNextPageRequested(const QString& url, int prevResults)
{
	if (_terminateRequested)
	{
		onScraperFinished();
		return;
	}

	_webEngineView->load(QUrl(url));
	_ui->console->appendPlainText("Loading next page...");
	_totalResultsDone += prevResults;
}

void MainWindow::onScraperFinished()
{
	_scraper->deleteLater();

	_scrapeStarted = false;

	_ui->terminateButton->setEnabled(false);
	_ui->parsePageButton->setEnabled(true);
	_ui->progressBar->setValue(0);
	_ui->progressBar->setFormat("%p%");
	_ui->progressBar->setEnabled(false);
	_ui->statusLineEdit->setText("Unknown");
	_ui->statusLineEdit->setStyleSheet("background-color: grey;");
	_ui->console->appendPlainText("Finished");
}

void MainWindow::onTerminateRequested()
{
	_terminateRequested = true;
	_ui->terminateButton->setEnabled(false);
}

void MainWindow::showCleanupWindow()
{
	static Cleanup* cleanupWindow = nullptr;

	if (cleanupWindow) return;

	cleanupWindow = new Cleanup();
	cleanupWindow->setAttribute(Qt::WA_DeleteOnClose);
	cleanupWindow->show();
	QObject::connect(cleanupWindow, &Cleanup::destroyed, [=]
	{
		//Avoid dangling pointers
		cleanupWindow = nullptr;
	});
}
