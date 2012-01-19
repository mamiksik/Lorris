/****************************************************************************
**
**    This file is part of Lorris.
**    Copyright (C) 2012 Vojtěch Boček
**
**    Contact: <vbocek@gmail.com>
**             https://github.com/Tasssadar
**
**    Lorris is free software: you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation, either version 3 of the License, or
**    (at your option) any later version.
**
**    Lorris is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
**    You should have received a copy of the GNU General Public License
**    along with Lorris.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include <QDockWidget>
#include <QMdiArea>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QListView>
#include <QStringListModel>
#include <QSpinBox>
#include <QSlider>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "lorrisanalyzer.h"
#include "sourcedialog.h"
#include "ui_lorrisanalyzer.h"
#include "packet.h"
#include "analyzerdatastorage.h"
#include "devicetabwidget.h"
#include "analyzerdataarea.h"
#include "connection/connectionmgr.h"
#include "DataWidgets/datawidget.h"
#include "DataWidgets/numberwidget.h"
#include "DataWidgets/barwidget.h"
#include "DataWidgets/colorwidget.h"
#include "DataWidgets/GraphWidget/graphwidget.h"
#include "sourceselectdialog.h"

LorrisAnalyzer::LorrisAnalyzer() : WorkTab(),ui(new Ui::LorrisAnalyzer)
{
    ui->setupUi(this);

    connect(ui->connectButton,   SIGNAL(clicked()),         SLOT(connectButton()));
    connect(ui->addSourceButton, SIGNAL(clicked()),         SLOT(onTabShow()));
    connect(ui->saveDataButton,  SIGNAL(clicked()),         SLOT(saveDataButton()));
    connect(ui->collapseTop,     SIGNAL(clicked()),         SLOT(collapseTopButton()));
    connect(ui->collapseRight,   SIGNAL(clicked()),         SLOT(collapseRightButton()));
    connect(ui->clearButton,     SIGNAL(clicked()),         SLOT(clearButton()));
    connect(ui->timeSlider,      SIGNAL(valueChanged(int)), SLOT(timeSliderMoved(int)));
    connect(ui->timeBox,         SIGNAL(valueChanged(int)), SLOT(timeBoxChanged(int)));

    m_storage = new AnalyzerDataStorage(this);

    m_dev_tabs = new DeviceTabWidget(this);
    m_dev_tabs->addDevice();
    ui->leftVLayout->insertWidget(1, m_dev_tabs);

    connect(this, SIGNAL(newData(analyzer_data*)), m_dev_tabs, SLOT(handleData(analyzer_data*)));
    connect(m_dev_tabs, SIGNAL(updateData()), this, SLOT(updateData()));

    m_data_area = new AnalyzerDataArea(this);
    ui->bottomHLayout->insertWidget(0, m_data_area, 4);
    ui->bottomHLayout->setStretch(1, 1);
    connect(m_data_area, SIGNAL(updateData()), this, SLOT(updateData()));
    connect(m_data_area, SIGNAL(mouseStatus(bool,data_widget_info)), this, SLOT(widgetMouseStatus(bool,data_widget_info)));

    QWidget *tmp = new QWidget(this);
    QVBoxLayout *widgetBtnL = new QVBoxLayout(tmp);
    widgetBtnL->addWidget(new NumberWidgetAddBtn(tmp));
    widgetBtnL->addWidget(new BarWidgetAddBtn(tmp));
    widgetBtnL->addWidget(new ColorWidgetAddBtn(tmp));
    widgetBtnL->addWidget(new GraphWidgetAddBtn(tmp));

    widgetBtnL->addWidget(new QWidget(tmp), 4);
    ui->widgetsScrollArea->setWidget(tmp);

    m_packet = NULL;
    m_state = 0;
    highlightInfoNotNull = false;
    m_curData = NULL;
}

LorrisAnalyzer::~LorrisAnalyzer()
{
    delete ui;
    delete m_storage;
    if(m_packet)
    {
        delete m_packet->header;
        delete m_packet;
    }
    delete m_dev_tabs;
    delete m_curData;
}

void LorrisAnalyzer::connectButton()
{
    if(m_state & STATE_DISCONNECTED)
    {
        ui->connectButton->setText(tr("Connecting..."));
        ui->connectButton->setEnabled(false);
        connect(m_con, SIGNAL(connectResult(Connection*,bool)), this, SLOT(connectionResult(Connection*,bool)));
        m_con->OpenConcurrent();
    }
    else
    {
        m_con->Close();
        m_state |= STATE_DISCONNECTED;

        ui->connectButton->setText(tr("Connect"));
    }
}

void LorrisAnalyzer::connectionResult(Connection */*con*/,bool result)
{
    disconnect(m_con, SIGNAL(connectResult(Connection*,bool)), this, 0);
    ui->connectButton->setEnabled(true);
    if(!result)
    {
        ui->connectButton->setText(tr("Connect"));

        QMessageBox *box = new QMessageBox(this);
        box->setIcon(QMessageBox::Critical);
        box->setWindowTitle(tr("Error!"));
        box->setText(tr("Can't open connection!"));
        box->exec();
        delete box;
    }
}

void LorrisAnalyzer::connectedStatus(bool connected)
{
    if(connected)
    {
        m_state &= ~(STATE_DISCONNECTED);
        ui->connectButton->setText(tr("Disconnect"));
    }
    else
    {
        m_state |= STATE_DISCONNECTED;
        ui->connectButton->setText(tr("Connect"));
    }
}

void LorrisAnalyzer::readData(const QByteArray& data)
{
    if((m_state & STATE_DIALOG) != 0 || !m_packet)
        return;

    if(!m_curData)
        m_curData = new analyzer_data(m_packet);

    QByteArray dta = data;
    static bool first = true;
    quint32 curRead = 1;

    while(dta.length() != 0)
    {
        if(first || curRead == 0)
        {
            int index = dta.indexOf(m_curData->getStaticData());
            if(index == -1)
                break;
            dta.remove(0, index);
            first = false;
        }
        curRead = m_curData->addData(dta);
        dta.remove(0, curRead);
        if(m_curData->isValid())
        {
            m_storage->addData(m_curData);
            m_curData = new analyzer_data(m_packet);
        }
    }

    bool update = ui->timeSlider->value() == ui->timeSlider->maximum();
    ui->timeSlider->setMaximum(m_storage->getSize());
    ui->timeBox->setMaximum(m_storage->getSize());
    ui->timeBox->setSuffix(tr(" of ") + QString::number(m_storage->getSize()));
    if(update)
    {
        ui->timeSlider->setValue(m_storage->getSize());
        ui->timeBox->setValue(m_storage->getSize());
    }
}

void LorrisAnalyzer::onTabShow()
{
    m_state |= STATE_DIALOG;
    SourceSelectDialog *s = new SourceSelectDialog(this);

    if(m_con->getType() == CONNECTION_FILE)
        s->DisableNew();

    qint8 res = s->get();
    switch(res)
    {
        case -1:
            m_state &= ~(STATE_DIALOG);
            break;
        case 0:
        {
            QString file = s->getFileName();
            quint8 mask = s->getDataMask();
            load(&file, mask);
            break;
        }
        case 1:
        {
            SourceDialog *d = new SourceDialog(this);
            m_state |= STATE_DIALOG;
            connect(this->m_con, SIGNAL(dataRead(QByteArray)), d, SLOT(readData(QByteArray)));

            analyzer_packet *packet = d->getStructure();
            delete d;

            {
                m_state &= ~(STATE_DIALOG);
                if(!packet)
                    return;

                m_curData = new analyzer_data(packet);

                if(m_packet)
                {
                    delete m_packet->header;
                    delete m_packet;
                }
                m_data_area->clear();
                m_storage->Clear();
                m_dev_tabs->removeAll();
                m_dev_tabs->setHeader(packet->header);
                m_dev_tabs->addDevice();

                m_storage->setPacket(packet);
                m_packet = packet;
            }
            break;
        }
    }
    delete s;
}

void LorrisAnalyzer::timeSliderMoved(int value)
{
    if(value != 0)
        updateData();

    if(ui->timeSlider->isSliderDown())
        ui->timeBox->setValue(value);
}

void LorrisAnalyzer::timeBoxChanged(int value)
{
    ui->timeSlider->setValue(value);
}

void LorrisAnalyzer::updateData()
{
    int val = ui->timeSlider->value();

    if(val != 0 && (quint32)val <= m_storage->getSize())
        emit newData(m_storage->get(val-1));
}

void LorrisAnalyzer::load(QString *name, quint8 mask)
{
    analyzer_packet *packet = m_storage->loadFromFile(name, mask, m_data_area, m_dev_tabs);
    if(!packet)
        return;

    // old packet deleted in AnalyzerDataStorage::loadFromFile()
    m_packet = packet;
    m_curData = new analyzer_data(m_packet);

    if(!m_dev_tabs->count())
    {
        m_dev_tabs->removeAll();
        m_dev_tabs->setHeader(packet->header);
        m_dev_tabs->addDevice();
    }

    ui->timeSlider->setMaximum(m_storage->getSize());
    ui->timeSlider->setValue(m_storage->getSize());
    ui->timeBox->setMaximum(m_storage->getSize());
    ui->timeBox->setSuffix(tr(" of ") + QString::number(m_storage->getSize()));
    ui->timeBox->setValue(m_storage->getSize());
    m_state &= ~(STATE_DIALOG);
}

void LorrisAnalyzer::saveDataButton()
{
    m_storage->SaveToFile(m_data_area, m_dev_tabs);
}

void LorrisAnalyzer::widgetMouseStatus(bool in, const data_widget_info &info)
{
    if(in)
    {
        if(highlightInfoNotNull && highlightInfo != info)
            m_dev_tabs->setHighlightPos(highlightInfo, false);

        bool found = m_dev_tabs->setHighlightPos(info, true);

        if(found)
        {
            highlightInfo = info;
            highlightInfoNotNull = true;
            return;
        }
    }

    if(highlightInfoNotNull)
    {
        m_dev_tabs->setHighlightPos(highlightInfo, false);
        highlightInfoNotNull = false;
    }
}

void LorrisAnalyzer::collapseTopButton()
{
    setAreaVisibility(AREA_TOP, !isAreaVisible(AREA_TOP));
}

void LorrisAnalyzer::collapseRightButton()
{
    setAreaVisibility(AREA_RIGHT, !isAreaVisible(AREA_RIGHT));
}

bool LorrisAnalyzer::isAreaVisible(quint8 area)
{
    switch(area)
    {
        case AREA_TOP:   return m_dev_tabs->isVisible();
        case AREA_RIGHT: return ui->widgetsScrollArea->isVisible();
    }
    return false;
}

void LorrisAnalyzer::setAreaVisibility(quint8 area, bool visible)
{
    if(area & AREA_TOP)
    {
        if(visible)
        {
            m_dev_tabs->show();
            ui->collapseTop->setText("^");
        }
        else
        {
            m_dev_tabs->hide();
            ui->collapseTop->setText("v");
        }
    }
    if(area & AREA_RIGHT)
    {
        if(visible)
        {
            ui->widgetsScrollArea->show();
            ui->collapseRight->setText(">");
        }
        else
        {
            ui->widgetsScrollArea->hide();
            ui->collapseRight->setText("<");
        }
    }
}

void LorrisAnalyzer::clearButton()
{
    QMessageBox *box = new QMessageBox(this);
    box->setWindowTitle(tr("Clear data?"));
    box->setText(tr("Do you really clear data, widgets and packet structure?"));
    box->addButton(tr("Yes"), QMessageBox::YesRole);
    box->addButton(tr("No"), QMessageBox::NoRole);
    box->setIcon(QMessageBox::Question);
    int ret = box->exec();
    delete box;

    if(ret)
        return;

    analyzer_packet *packet = m_packet;
    m_packet = NULL;

    m_dev_tabs->removeAll();
    m_dev_tabs->setHeader(NULL);
    m_dev_tabs->addDevice();

    m_data_area->clear();

    m_curData = NULL;
    m_storage->Clear();
    m_storage->setPacket(NULL);

    ui->timeSlider->setMaximum(0);
    ui->timeBox->setMaximum(0);

    if(packet)
    {
        delete packet->header;
        delete packet;
    }

    setAreaVisibility(AREA_TOP | AREA_RIGHT, true);
}