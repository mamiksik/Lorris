#include <QHBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

#include "qserialdeviceenumerator/serialdeviceenumerator.h"
#include "qserialdevice/abstractserial.h"
#include "WorkTabMgr.h"
#include "WorkTabInfo.h"
#include "WorkTab.h"
#include "tabdialog.h"
#include "mainwindow.h"
#include "connection/serialport.h"

TabDialog::TabDialog(QWidget *parent) : QDialog(parent, Qt::WindowFlags(0))
{
    setFixedSize(500, 200);

    firstLine = new QHBoxLayout;
    secondLine = new QHBoxLayout;

    pluginsBox = new QComboBox;
    std::vector<WorkTabInfo*> *tabs = sWorkTabMgr.GetWorkTabInfos();
    for(std::vector<WorkTabInfo*>::iterator i = tabs->begin(); i != tabs->end(); ++i)
        pluginsBox->addItem((*i)->GetName());
    connect(pluginsBox, SIGNAL(currentIndexChanged(int)), this, SLOT(PluginSelected(int)));

    conBox = new QComboBox;
    connect(conBox, SIGNAL(currentIndexChanged(int)), this, SLOT(FillConOptions(int)));
    PluginSelected(pluginsBox->currentIndex());

    QPushButton *create = new QPushButton("Create", this);
    create->setObjectName("CreateButton");
    connect(create, SIGNAL(clicked()), this, SLOT(CreateTab()));

    firstLine->addWidget(pluginsBox);
    firstLine->addWidget(conBox);

    layout = new QVBoxLayout(this);
    layout->addLayout(firstLine);
    layout->addLayout(secondLine);
    layout->addWidget(create);
    setLayout(layout);
}

TabDialog::~TabDialog()
{
    WorkTab::DeleteAllMembers(layout);
    delete layout; 
}

void TabDialog::PluginSelected(int index)
{
    std::vector<WorkTabInfo*> *tabs = sWorkTabMgr.GetWorkTabInfos();
    quint8 conn = tabs->at(index)->GetConType();

    conBox->clear();
    if(conn & CON_MSK(CONNECTION_SOCKET))      conBox->addItem("Socket", QVariant((int)CONNECTION_SOCKET));
    if(conn & CON_MSK(CONNECTION_SERIAL_PORT)) conBox->addItem("Serial port", QVariant(CONNECTION_SERIAL_PORT));
    if(conn & CON_MSK(CONNECTION_FILE))        conBox->addItem("File", QVariant((int)CONNECTION_FILE));
}

void TabDialog::FillConOptions(int index)
{
    WorkTab::DeleteAllMembers(secondLine);

    switch(conBox->itemData(index).toInt())
    {
        case CONNECTION_SOCKET:
        case CONNECTION_FILE:
        default:   // TODO
            return;
        case CONNECTION_SERIAL_PORT:
        {
            QLabel *portLabel = new QLabel("Port: ", NULL, Qt::WindowFlags(0));
            QComboBox *portBox = new QComboBox(this);
            portBox->setEditable(true);
            portBox->setObjectName("PortBox");

            m_sde = SerialDeviceEnumerator::instance();
            foreach (QString s, m_sde->devicesAvailable())
            {
                m_sde->setDeviceName(s);
                portBox->addItem(m_sde->name(), m_sde->name());
            }
            m_sde->setEnabled(false);
            SerialDeviceEnumerator::destroyInstance();
            m_sde = NULL;

            secondLine->addWidget(portLabel);
            secondLine->addWidget(portBox);

            QLabel *rateLabel = new QLabel("Baud Rate: ", NULL, Qt::WindowFlags(0));
            QComboBox *rateBox = new QComboBox(this);
            rateBox->addItem("38400",   AbstractSerial::BaudRate38400);
            rateBox->addItem("19200",   AbstractSerial::BaudRate19200);
            rateBox->addItem("57600",   AbstractSerial::BaudRate57600);
            rateBox->addItem("115200",  AbstractSerial::BaudRate115200);
            rateBox->addItem("500000",  AbstractSerial::BaudRate500000);
            rateBox->addItem("1000000", AbstractSerial::BaudRate1000000);
            rateBox->addItem("1500000", AbstractSerial::BaudRate1500000);
            rateBox->addItem("2000000", AbstractSerial::BaudRate2000000);
            rateBox->setObjectName("RateBox");
            secondLine->addWidget(rateLabel);
            secondLine->addWidget(rateBox);
            break;
        }
    }
}

void TabDialog::CreateTab()
{
    WorkTabInfo *info = sWorkTabMgr.GetWorkTabInfos()->at(pluginsBox->currentIndex());
    WorkTab *tab = NULL;

    switch(conBox->itemData(conBox->currentIndex()).toInt())
    {
        case CONNECTION_SERIAL_PORT:
            tab = ConnectSP(info);
            if(!tab)
                return;
            break;
        default:    // TODO: other connection types
            return;
    }
    close();
}

WorkTab *TabDialog::ConnectSP(WorkTabInfo *info)
{
    QString portName("");
    AbstractSerial::BaudRate rate;

    QComboBox *combo = findChild<QComboBox *>("PortBox");
    portName = combo->currentText();
    combo = findChild<QComboBox *>("RateBox");
    rate =  AbstractSerial::BaudRate(combo->itemData(combo->currentIndex()).Int);

    SerialPort *port = (SerialPort*)sConMgr.FindConnection(CONNECTION_SERIAL_PORT, portName);
    if(port)
    {
        WorkTab *tab = info->GetNewTab();
        sWorkTabMgr.AddWorkTab(tab, info->GetName() + " - " +port->GetIDString());
        tab->setConnection(port);
        return tab;
    }
    else
    {
        QPushButton *create = findChild<QPushButton *>("CreateButton");
        create->setText("Connecting...");
        create->setEnabled(false);

        tmpTabInfo = info;

        port = new SerialPort();
        port->SetNameAndRate(portName, rate);

        connect(port, SIGNAL(connectResult(Connection*,bool)), this, SLOT(serialConResult(Connection*,bool)));
        port->OpenConcurrent();
        return NULL;
    }
}

void TabDialog::serialConResult(Connection *con, bool result)
{
    QPushButton *create = findChild<QPushButton *>("CreateButton");
    disconnect(con, SIGNAL(connectResult(Connection*,bool)), this, 0);

    if(result)
    {
        WorkTab *tab = tmpTabInfo->GetNewTab();
        sWorkTabMgr.AddWorkTab(tab, tmpTabInfo->GetName() + " - " + con->GetIDString());
        tab->setConnection(con);
        sConMgr.AddCon(CONNECTION_SERIAL_PORT, con);
        close();
    }
    else
    {
        create->setText("Create");
        create->setEnabled(true);

        QMessageBox *box = new QMessageBox(this);
        box->setWindowTitle("Error!");
        box->setText("Error opening serial port!");
        box->setIcon(QMessageBox::Critical);
        box->exec();
        delete box;
        delete con;
    }
    tmpTabInfo = NULL;
}

