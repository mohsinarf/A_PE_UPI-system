#include "laserController.h"

#define ECHO_WAIT 100
#define ECHO_WAIT_FOR_CURRENT 100


laserController::laserController(structScan *laserInfoPtrArg, QString portName)
{
    serial = new QSerialPort;
    initLaserControllerDone = false;

    laserInfoPtr = laserInfoPtrArg;
    LaserComPort= "COM8";

//    if (portName !=NULL)
       initLaserController();
}

laserController::~laserController()
{
    serial->close();
}

bool laserController::initLaserController()
{
    QByteArray InitComArray[6];
    QString StatusTip;
    int i;
    int waitForEcho = 150;
    qDebug()<<"initLaserController()";
    //Populate Init  Command Array
    InitComArray[0] = "8000000080\r";//link check
    InitComArray[1] = "8200000082\r";//Internal trig
    InitComArray[2] = "8C0100008D\r";//QS
    InitComArray[3] = "8400000084\r";//Laser off

    {
        //QString laserPRFCmd = ("PRF="+(QString::number(laserInfoPtr->PRF.toDouble()))+"\r\n");
        QString laserPRFCmd = ("A60001F453\r");// PRF 10k A600271091 5k A60013883D 8k A6001F40F9 9k A6002328AD 1k A60003E84D 0.5k A60001F453
        InitComArray[4] = laserPRFCmd.toLocal8Bit();
    }

    {
        QString laserCurrentCmd = ("88040028A4\r");//100% 88040064E8 0% 880400008C
        InitComArray[5] = laserCurrentCmd.toLocal8Bit();
    }
//    InitComArray[7] = "DIO=1\r\n";

    for (i = 0; i<5; i++)
    {
        waitForEcho =  (i!=5) ? ECHO_WAIT : ECHO_WAIT_FOR_CURRENT;

        if( rwSerialPort(LaserComPort, InitComArray[i],waitForEcho ) == false)
        {
            initLaserControllerDone = false;
            emit updateProgressBar_laserControllerSignal(100);
            return false;
        }
        emit updateProgressBar_laserControllerSignal((i*100)/6);
    }
    serial->close();

    initLaserControllerDone = true;
    return true;
}

void laserController::initSerialPort()
{
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
}

bool laserController::setPRF()
{
    updateStatusBar_laserControllerSignal("Please wait while setting laser PRF.");
    double d_laserPRF = 1000*laserInfoPtr->PRF.toDouble();
    QString PRF = QString::number(d_laserPRF);
//    QString laser_PRF = "PRF="+PRF+"\r\n";
    QString laser_PRF = "A60001F453\r"; //always set to 50 later-on the scan system will dictate the prf.
    QByteArray data = laser_PRF.toLocal8Bit();
    return(rwSerialPort(LaserComPort,data,ECHO_WAIT));
}

bool laserController::setCurrent()
{
    updateStatusBar_laserControllerSignal("Please wait while setting laser current.");
    int d_lasercurrent = laserInfoPtr->Current.toInt();
    QString laser_current;
    switch (d_lasercurrent)
    {
        case  0: laser_current = "880400008C\r";break;
        case 10: laser_current = "8804000A86\r";break;
        case 20: laser_current = "8804001498\r";break;
        case 30: laser_current = "8804001E92\r";break;
        case 40: laser_current = "88040028A4\r";break;
        case 50: laser_current = "88040032BE\r";break;
        case 60: laser_current = "8804003CB0\r";break;
        case 70: laser_current = "88040046CA\r";break;
        case 80: laser_current = "88040050DC\r";break;
        case 90: laser_current = "8804005AD6\r";break;
        case 100: laser_current = "88040064E8\r";break;
        default: laser_current = "88040028A4\r";break;
    }


    QByteArray data = laser_current.toLocal8Bit();
    return(rwSerialPort(LaserComPort,data,ECHO_WAIT_FOR_CURRENT));
}

bool laserController::rwSerialPort(QString portName, QByteArray data, int waitForEcho, QString* qsRsp)
{
    bool SerialPortOpen;
    qint64 WrittenBytes,numRead;
    char response[20] ={'\0'} ;
    int ReadIndex = 0;

    serial->setPortName(portName);

    if(SerialPortOpen = serial->open(QIODevice::ReadWrite))
        {
            initSerialPort();

            WrittenBytes = serial->write(data);

            QTime myTimer;
            myTimer.start();
            //read response
            ReadIndex = 0;
            for (int j = 0; j<20; j ++) response[j] ='\0';
            do
            {
                serial->waitForReadyRead(waitForEcho);
                numRead  = serial->read(&response[ReadIndex], 20);
                ReadIndex += numRead;
            }
            while( (response[(ReadIndex-1)]!='\n') && numRead!=0 );

            if(ReadIndex == 0)
            {
              // no response from port somethings wrong, quit further commands and try manual command exec
              qDebug()<<"No Echo from Serial Port" << portName;
              serial->close();
              return false;
            }
            else
            {
//                data.chop(1);
//                response[ReadIndex-1] = response[ReadIndex-2] = '\0';
                qDebug().nospace()<<"Command: "<<data<<"      "<<"\t\t-Response: "<<response<<"\t-Response Time(ms): "<<myTimer.elapsed();
                if (qsRsp != NULL)
                {
                    qsRsp->clear();
                    int i = 0;
                    while(response[i]!='\0')
                        qsRsp->append(response[i++]);
                }
            }
        }
        else
        {
            qDebug()<<"Could not open PortNumber:"<<portName<<"SerialPortOpen: "<<SerialPortOpen;
            serial->close();
            return false;
        }

    serial->close();
    return true;
}
bool laserController::srchPortandConnect(QString comPortHint, QString* laserComPortSel)
{
    bool portAccessResult = false;
    this->LaserComPort = comPortHint;

//    QByteArray data("?DIO\r\n"); //enquire diode status to see if this is the actual laser controller
    QByteArray data("8000000080\r"); //enquire link status to see if this is the actual laser controller
    for (int xx=0; xx<3; xx++)
        portAccessResult = rwSerialPort(LaserComPort,data,ECHO_WAIT);

    if (portAccessResult == true)
    {
        laserComPortSel->append(this->LaserComPort);
        qDebug()<<"LaserController port found successfully:"<<*laserComPortSel;
        return portAccessResult;
    }
    else
    { //check other ports
        foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
        {
            if (info.portName() != "COM1" && info.portName() != comPortHint )
            {
                this->LaserComPort = info.portName();
//                portAccessResult = rwSerialPort(LaserComPort,data,ECHO_WAIT);
                if (portAccessResult == true)
                {
                    laserComPortSel->append(this->LaserComPort);
                    qDebug()<<"LaserController port found successfully: "<<*laserComPortSel;
                    return portAccessResult;
                }
            }
        }
    }

    qDebug()<<"Laser Controller Port not found";
    return portAccessResult;
}

bool laserController::onEXT()
{
    QByteArray data("8201000083\r");
    return(rwSerialPort(LaserComPort,data,ECHO_WAIT));
}

bool laserController::offEXT()
{
    bool success = false;
    QString response;
    QByteArray data("8200000082\r");
    do{
          success = rwSerialPort(LaserComPort,data,ECHO_WAIT,&response);
      }
      while(response[0] == 'B');
      return(success);
}

bool laserController::onSHT()
{
    bool success = false;
    QString response;
    QByteArray data("8401000085\r");
    do{
        Sleep(50);
        success = rwSerialPort(LaserComPort,data,ECHO_WAIT,&response);
      }
      while(response[0] == 'B');
      return(success);
}

bool laserController::offSHT()
{
    bool success = false;
    QString response;
    QByteArray data("8400000084\r");
    do{
        Sleep(50);
        success = rwSerialPort(LaserComPort,data,ECHO_WAIT,&response);
      }
      while(response[0] == 'B');
      return(success);
}

bool laserController::offDIO()
{
    bool success = false;
    QString response;
    QByteArray data("8000000080\r");
    do{
        Sleep(50);
          success = rwSerialPort(LaserComPort,data,ECHO_WAIT,&response);
      }
      while(response[0] == 'B');

      return(success);
}
bool laserController::onDIO()
{
    QByteArray data("8000000080\r");
    return(rwSerialPort(LaserComPort,data,ECHO_WAIT));
}

bool laserController::offQS()
{
    bool success = false;
    QString response;
    QByteArray data("8000000080\r");

      do{
        Sleep(50);
            success = rwSerialPort(LaserComPort,data,ECHO_WAIT,&response);
        }
        while(response[0] == 'B');

    return(success);
}
bool laserController::onQS()
{
    QByteArray data("8000000080\r");
    return(rwSerialPort(LaserComPort,data,ECHO_WAIT));
}



bool laserController::getCurrent(QString *sCurrent)
{
    bool success = false;
    QByteArray data("8000000080\r");
    success = rwSerialPort(LaserComPort,data,ECHO_WAIT,sCurrent);
    return(success);
}
