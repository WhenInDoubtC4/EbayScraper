#include "Scraper.h"

#include <QDebug>

Scraper::Scraper(QObject* parent, const QString& dataFilePath) : QObject(parent)
{
	_filePath = dataFilePath;

	//Create file and write header
	QFile dataFile(_filePath);
	dataFile.open(QIODevice::WriteOnly | QIODevice::Text);
	QTextStream dataFileStream(&dataFile);
	dataFileStream << "ID" << CSV_SEPARATOR
				   << "Name" << CSV_SEPARATOR
				   << "Price" << CSV_SEPARATOR
				   << "Quantity" << CSV_SEPARATOR
				   << "Date\n";
	dataFile.close();
}

void Scraper::scrapePage(const QString& html)
{
	emit log("Extracting IDs from search page...");

	_purchaseResults.clear();
	QList<QString> ids;

	auto htmlDocument = QGumboDocument::parse(html);
	QGumboNode rootNode = htmlDocument.rootNode();

	//Next page
	QGumboNodes nextButtonNodes = rootNode.getElementsByClassName("pagn-next");
	if (nextButtonNodes.size() > 0)
	{
		QGumboNode nextButtonNode = nextButtonNodes[0].getElementsByTagName(HtmlTag::A)[0];
		_nextButtonDisabled = nextButtonNode.hasAttribute("aria-disabled");
		_nextPageUrl = nextButtonNode.getAttribute("href");
	}
	else
	{
		_nextPageUrl = QString();
	}

	QGumboNodes listView = rootNode.getElementById("ListViewInner");

	//Stalled, captcha?
	if (listView.size() == 0)
	{
		emit stalled();
		return;
	}

	QGumboNodes resultNodes = listView[0].childNodes();
	for (QGumboNode resultRootNode : resultNodes)
	{
		//Check for ghost element
		QGumboNodes titleRootNodes = resultRootNode.getElementsByClassName("lvtitle");
		if (titleRootNodes.empty()) continue;

		const QString id = resultRootNode.getAttribute("listingid");

		ids << id;
	}

	_resultCount = ids.size();
	emit log("Extracted " + QString::number(_resultCount));

	for (QString id : qAsConst(ids))
	{
		emit log("Loading pruchase history for ID " + id);

		auto purchaseHistoryPage = new QWebEnginePage(QWebEngineProfile::defaultProfile());
		purchaseHistoryPage->load(QUrl(PURCHASE_HISTORY_URL + id));
		QObject::connect(purchaseHistoryPage, &QWebEnginePage::loadFinished, [=]
		{
			//This looks ugly
			purchaseHistoryPage->toHtml(std::bind(&Scraper::processPurchaseHistoryPage, this, std::placeholders::_1, purchaseHistoryPage, id));
		});
	}
}

void Scraper::processPurchaseHistoryPage(const QString& html, QWebEnginePage* page, const QString& id)
{
	static int currentResults = 0;

	auto purchaseHistoryDocument = QGumboDocument::parse(html);
	QGumboNode rootNode = purchaseHistoryDocument.rootNode();

	QGumboNodes titleNodes = rootNode.getElementsByClassName("medium-product-title");

	//Extract purchase history if there is any
	if (!titleNodes.empty())
	{
		QGumboNode historyTable = rootNode.getElementsByClassName("app-table__table")[0];
		QGumboNodes rows = historyTable.getElementsByClassName("app-table__row");

		int priceIndex = 1;
		int quantityIndex = 2;
		int dateIndex = 3;

		QGumboNodes headers = historyTable.getElementsByTagName(HtmlTag::TH);
		for (unsigned int i = 0; i < headers.size(); i++)
		{
			const QString headerTitle = headers[i].getElementsByTagName(HtmlTag::SPAN)[1].innerText();
			if (headerTitle.contains("price", Qt::CaseInsensitive)) priceIndex = i;
			if (headerTitle.contains("quantity", Qt::CaseInsensitive)) quantityIndex = i;
			if (headerTitle.contains("date", Qt::CaseInsensitive)) dateIndex = i;
		}

		for (QGumboNode row : rows)
		{
			//0: username, 1: price, 2: quantity, 3: date
			//<td><div><span><span>Actual data here</span></span></div></td> confused yet?
			QGumboNodes data = row.getElementsByTagName(HtmlTag::TD);

			purchaseHistoryResult result;
			result.id = id;
			result.name = titleNodes[0].childNodes()[0].innerText();
			result.price = data[priceIndex].getElementsByTagName(HtmlTag::SPAN)[1].innerText();
			result.quantity = data[quantityIndex].getElementsByTagName(HtmlTag::SPAN)[1].innerText().toInt();
			result.date = data[dateIndex].getElementsByTagName(HtmlTag::SPAN)[1].innerText();
			_purchaseResults << result;

			emit log("> ID: " + id + ", NAME: \"" + result.name + "\", PRICE: " + result.price + ", QUANTITY: " + QString::number(result.quantity) + ", DATE: " + result.date);
		}

	}

	page->deleteLater();

	emit updateProgress(++currentResults);

	if (currentResults == _resultCount)
	{
		emit log("DONE, saving data and requesting next page");

		if (!_nextButtonDisabled)
		{
			//Save data
			QFile dataFile(_filePath);
			dataFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
			QTextStream dataFileStream(&dataFile);
			for (purchaseHistoryResult result : qAsConst(_purchaseResults))
			{
				dataFileStream << result.id << CSV_SEPARATOR
							   << result.name << CSV_SEPARATOR
							   << result.price << CSV_SEPARATOR
							   << result.quantity << CSV_SEPARATOR
							   << result.date << CSV_SEPARATOR
							   << "\n";
			}
			dataFile.close();

			emit requestNextPage(_nextPageUrl, currentResults);
			currentResults = 0;
		}
		else
		{
			emit finished();
		}
	}
}
