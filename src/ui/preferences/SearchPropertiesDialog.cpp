#include "SearchPropertiesDialog.h"

#include "ui_SearchPropertiesDialog.h"

#include <QtCore/QUrlQuery>
#include <QtWidgets/QFileDialog>

namespace Otter
{

SearchPropertiesDialog::SearchPropertiesDialog(const QVariantHash &engineData, const QStringList &shortcuts, QWidget *parent) : QDialog(parent),
	m_engineData(engineData),
	m_ui(new Ui::SearchPropertiesDialog)
{
	m_ui->setupUi(this);
	m_ui->iconButton->setIcon(engineData["icon"].value<QIcon>());
	m_ui->titleLineEdit->setText(engineData["title"].toString());
	m_ui->descriptionLineEdit->setText(engineData["description"].toString());
	m_ui->shortcutLineEdit->setText(engineData["shortcut"].toString());
	m_ui->shortcutLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression((shortcuts.isEmpty() ? QString() : QString("(?!\\b(%1)\\b)").arg(shortcuts.join('|'))) + "[a-z0-9]*"), m_ui->shortcutLineEdit));
	m_ui->defaultSearchCheckBox->setChecked(engineData["isDefault"].toBool());

	connect(m_ui->resultsPostMethodCheckBox, SIGNAL(toggled(bool)), m_ui->resultsPostWidget, SLOT(setEnabled(bool)));
	connect(m_ui->suggestionsPostMethodCheckBox, SIGNAL(toggled(bool)), m_ui->suggestionsPostWidget, SLOT(setEnabled(bool)));

	m_ui->resultsAddressLineEdit->setText(engineData["resultsUrl"].toString());
	m_ui->resultsPostMethodCheckBox->setChecked(engineData["resultsMethod"].toString() == "post");
	m_ui->resultsEnctypeComboBox->setCurrentText(engineData["resultsEnctype"].toString());

	const QList<QPair<QString, QString> > resultsParameters = QUrlQuery(engineData["resultsParameters"].toString()).queryItems();

	m_ui->resultsParametersWidget->setRowCount(resultsParameters.count());

	for (int i = 0; i < resultsParameters.count(); ++i)
	{
		m_ui->resultsParametersWidget->setItem(i, 0, new QTableWidgetItem(resultsParameters.at(i).first));
		m_ui->resultsParametersWidget->setItem(i, 1, new QTableWidgetItem(resultsParameters.at(i).second));
	}

	m_ui->suggestionsAddressLineEdit->setText(engineData["suggestionsUrl"].toString());
	m_ui->suggestionsPostMethodCheckBox->setChecked(engineData["suggestionsMethod"].toString() == "post");
	m_ui->suggestionsEnctypeComboBox->setCurrentText(engineData["suggestionsEnctype"].toString());

	const QList<QPair<QString, QString> > suggestionsParameters = QUrlQuery(engineData["suggestionsParameters"].toString()).queryItems();

	m_ui->suggestionsParametersWidget->setRowCount(suggestionsParameters.count());

	for (int i = 0; i < suggestionsParameters.count(); ++i)
	{
		m_ui->suggestionsParametersWidget->setItem(i, 0, new QTableWidgetItem(suggestionsParameters.at(i).first));
		m_ui->suggestionsParametersWidget->setItem(i, 1, new QTableWidgetItem(suggestionsParameters.at(i).second));
	}

	connect(m_ui->iconButton, SIGNAL(clicked()), this, SLOT(selectIcon()));
}

SearchPropertiesDialog::~SearchPropertiesDialog()
{
	delete m_ui;
}

void SearchPropertiesDialog::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);

	switch (event->type())
	{
		case QEvent::LanguageChange:
			m_ui->retranslateUi(this);

			break;
		default:
			break;
	}
}

void SearchPropertiesDialog::selectIcon()
{
	const QString path = QFileDialog::getOpenFileName(this, tr("Select Icon"), QString(), tr("Images (*.png *.jpg *.bmp *.gif *.ico)"));

	if (!path.isEmpty())
	{
		m_ui->iconButton->setIcon(QIcon(QPixmap(path)));
	}
}

QVariantHash SearchPropertiesDialog::getEngineData() const
{
	QVariantHash engineData = m_engineData;
	engineData["icon"] = m_ui->iconButton->icon();
	engineData["title"] = m_ui->titleLineEdit->text();
	engineData["description"] = m_ui->descriptionLineEdit->text();
	engineData["shortcut"] = m_ui->shortcutLineEdit->text();
	engineData["isDefault"] = m_ui->defaultSearchCheckBox->isChecked();
	engineData["resultsUrl"] = m_ui->resultsAddressLineEdit->text();
	engineData["resultsMethod"] = (m_ui->resultsPostMethodCheckBox->isChecked() ? "post" : "get");
	engineData["resultsEnctype"] = m_ui->resultsEnctypeComboBox->currentText();
	engineData["suggestionsUrl"] = m_ui->suggestionsAddressLineEdit->text();
	engineData["suggestionsMethod"] = (m_ui->suggestionsPostMethodCheckBox->isChecked() ? "post" : "get");
	engineData["suggestionsEnctype"] = m_ui->suggestionsEnctypeComboBox->currentText();

	return engineData;
}

}
