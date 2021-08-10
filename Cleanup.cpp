#include "Cleanup.h"
#include "ui_Cleanup.h"

#include <QDebug>

Cleanup::Cleanup(QWidget* parent) : QWidget(parent)
  , _ui(new Ui::Cleanup)
{
	_ui->setupUi(this);

	QObject::connect(_ui->sourceSelectButton, &QPushButton::pressed, this, &Cleanup::selectSourceFile);
	QObject::connect(_ui->outSelectButton, &QPushButton::pressed, this, &Cleanup::selectDestFile);
	QObject::connect(_ui->cancelButton, &QPushButton::pressed, this, &Cleanup::onCancelButtonPressed);
	QObject::connect(_ui->runButton, &QPushButton::pressed, this, &Cleanup::doCleanup);
	QObject::connect(this, &Cleanup::cleanupFinished, this, &Cleanup::onCleanupFinished);

	_networkAccessManager = new QNetworkAccessManager(this);
}

Cleanup::~Cleanup()
{
	delete _ui;
}

void Cleanup::selectSourceFile()
{
	const QString sourceFilePath = QFileDialog::getOpenFileName(this, "Select source file", "dataOut.csv", "CSV files(*.csv);;All files(*.*)");
	_ui->sourceLineEdit->setText(sourceFilePath);
}

void Cleanup::selectDestFile()

{
	const QString destFilePath = QFileDialog::getSaveFileName(this, "Select data out", "dataOut_cleaned.csv", "CSV files(*.csv);;All files(*.*)");
	_ui->outLineEdit->setText(destFilePath);
}

void Cleanup::onCancelButtonPressed()
{
	if (_cleanupRunning)
	{
		_requestTerminate = true;
	}
	else
	{
		close();
	}
}

bool Cleanup::ebayPriceToISO(QString ebayPrice, QString& outISOCode, QString& outPrice) const
{
	const QString price = ebayPrice
				.replace("US $", "USD")
				.replace("C $", "CAD")
				.replace("AU $", "AUD");

	QRegularExpression isoCodeRegex("[A-Z]+");
	outISOCode = isoCodeRegex.match(price).captured(0);

	QRegularExpression priceRegex("\\d+[,]?\\d+[.]\\d+");
	outPrice = priceRegex.match(price).captured(0).replace(",", "");

	return outPrice != QString();
}

QUrl Cleanup::getCurrencyConverterUrl(const QString& targetISOCode, const QString& amount, const QDate& date) const
{
	QString url = "https://markets.businessinsider.com/ajax/ExchangeRate_GetConversionForCurrenciesNumbers?isoCodeForeign=%1&isoCodeLocal=USD&amount=%2&date=%3";
	return QUrl(url.arg(targetISOCode, amount, date.toString(Qt::ISODate)));
}

void Cleanup::doCleanup()
{
	//Check file paths
	if (_ui->sourceLineEdit->text() == QString())
	{
		QMessageBox::critical(this, "Error", "No source file selected");
		return;
	}
	if (_ui->outLineEdit->text() == QString())
	{
		QMessageBox::critical(this, "Error", "No destination file selected");
		return;
	}

	_cleanupRunning = true;

	_ui->sourceLineEdit->setEnabled(false);
	_ui->sourceSelectButton->setEnabled(false);
	_ui->separatorComboBox->setEnabled(false);
	_ui->outLineEdit->setEnabled(false);
	_ui->outSelectButton->setEnabled(false);
	_ui->unshiftCheckBox->setEnabled(false);
	_ui->priceCheckBox->setEnabled(false);
	_ui->currencyCheckBox->setEnabled(false);
	_ui->dateCheckBox->setEnabled(false);
	_ui->quantityCheckBox->setEnabled(false);
	_ui->runButton->setEnabled(false);

	_ui->progressBar->setEnabled(true);

	const QString csvSeparator = _ui->separatorComboBox->currentText().replace("TAB", "\t");

	QFile dataFile(_ui->sourceLineEdit->text());
	dataFile.open(QIODevice::ReadOnly | QIODevice::Text);
	QTextStream dataFileStream(&dataFile);

	QFile destFile(_ui->outLineEdit->text());
	destFile.open(QIODevice::WriteOnly | QIODevice::Text);
	QTextStream destFileStream(&destFile);

	//Create file header
	destFileStream << "ID" << OUT_CSV_SEPARATOR
				   << "Name" << OUT_CSV_SEPARATOR
				   << "Price" << OUT_CSV_SEPARATOR
				   << "Quantity" << OUT_CSV_SEPARATOR
				   << "Date" << OUT_CSV_SEPARATOR
				   << "\n";

	//Get line count in file for progress calc and reset text stream
	_lineCount = 0;
	while (!dataFileStream.atEnd())
	{
		dataFileStream.readLine();
		_lineCount++;
	}
	dataFileStream.seek(0);

	int currentLine = 0;
	while (!dataFileStream.atEnd())
	{
		if (_requestTerminate)
		{
			_requestTerminate = false;
			break;
		}

		updateProgress(++currentLine);

		static bool ignoreHeader = true;
		if (ignoreHeader)
		{
			ignoreHeader = false;
			continue;
		}

		const QString line = dataFileStream.readLine();
		const QList<QString> data = line.split(csvSeparator);

		const int shift = data.size() > 6 ? 1 : 0;

		const QString id = data[0];
		const QString name = data[1 + shift];

		QString isoCode;
		QString price;
		const bool valid = ebayPriceToISO(data[2 + shift], isoCode, price);

		int quantity = data[3 + shift].toInt();

		//Remove timezone code and convert date
		QDateTime dateTime = EN_US_LOCALE.toDateTime(data[4 + shift].chopped(4), "d MMM yyyy 'at' h:mm:ssAP");

		if (!valid)
		{
			if (_ui->priceCheckBox->isChecked()) continue;

			//Set ISO code to USD to skip currency conversion
			isoCode = "USD";
			price = data[2 + shift];
		}

		//Currency conversion
		QString convertedPrice;
		if (_ui->currencyCheckBox->isChecked())
		{
			if (isoCode != "USD")
			{
				QNetworkRequest request(getCurrencyConverterUrl(isoCode, price, dateTime.date()));
				request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
				QNetworkReply* reply = _networkAccessManager->post(request, QByteArray());
				QEventLoop eventLoop;
				QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
				eventLoop.exec();
				if (reply->error() == QNetworkReply::NoError)
				{
					QJsonDocument replyJson = QJsonDocument::fromJson(reply->readAll());
					convertedPrice = replyJson["ConvertedAmountFourDigits"].toString();
				}
				else
				{
					//Revert to original
					convertedPrice = "ERR - " + isoCode + " " + price;
				}
			}
			else
			{
				convertedPrice = price;
			}
		}
		else
		{
			convertedPrice = data[2 + shift];
		}

		const int quantityRepeat = _ui->quantityCheckBox->isChecked() ? quantity : 1;
		const QString quantityString = _ui->quantityCheckBox->isChecked() ? "1" : QString::number(quantity);
		for (int i = 0; i < quantityRepeat; i++)
		{
			destFileStream << id << OUT_CSV_SEPARATOR;
			if (shift == 1 && !_ui->unshiftCheckBox->isChecked()) destFileStream << " " << OUT_CSV_SEPARATOR;
			destFileStream << name << OUT_CSV_SEPARATOR
						   << convertedPrice << OUT_CSV_SEPARATOR
						   << quantityString << OUT_CSV_SEPARATOR
						   << dateTime.toString(Qt::ISODate) << OUT_CSV_SEPARATOR
						   << "\n";

		}
	}

	dataFile.close();
	destFile.close();
	emit cleanupFinished();
}

void Cleanup::updateProgress(const int currentLine)
{
	float progress = float(currentLine) / float(_lineCount);
	_ui->progressBar->setValue(qRound(progress * 100.f));
	_ui->progressBar->setFormat(QString::number(progress * 100.f, 'f', 2) + "% (" + QString::number(currentLine) + "/" + QString::number(_lineCount) + ")");
}

void Cleanup::onCleanupFinished()
{
	_cleanupRunning = false;

	_ui->sourceLineEdit->setEnabled(true);
	_ui->sourceSelectButton->setEnabled(true);
	_ui->separatorComboBox->setEnabled(true);
	_ui->outLineEdit->setEnabled(true);
	_ui->outSelectButton->setEnabled(true);
	_ui->unshiftCheckBox->setEnabled(true);
	_ui->priceCheckBox->setEnabled(true);
	_ui->currencyCheckBox->setEnabled(true);
	_ui->dateCheckBox->setEnabled(true);
	_ui->quantityCheckBox->setEnabled(true);
	_ui->runButton->setEnabled(true);

	_ui->progressBar->setEnabled(false);
	_ui->progressBar->resetFormat();
	_ui->progressBar->setValue(0);
}
