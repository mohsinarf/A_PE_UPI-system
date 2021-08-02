#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    scanInfo.enableTT           = true; // true for tt and false for pe-upi
    setSettingStruct();
    ui->setupUi(this);

    this->setWindowTitle(QStringLiteral("Angular Scan Pulse Echo Ultrasonic Propogation Imaging System "));

    //progress bar
    ui->statusBar->addPermanentWidget(ui->progressBar,0);
    ui->progressBar->hide();
    ui->progressBar->setTextVisible(false);
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(100);
    ui->checkBoxEnableMultiBand->hide();
    ui->lineEditDaqVoltage->hide();
    ui->pushButtonDaqConfig->hide();
    ui->labelDaqVoltage->hide();
    ui->labelTrigDelay->hide();
    ui->lineEditDaqTrigDelay->hide();



    //hide&seek
    ui->mainToolBar->hide();
    //ui->menuBar->hide();
    ui->labelMovieDebug->hide();
    //ui->pushButtonSaveMovie->hide();
    ui->checkBoxTT->hide();

    //Tabwidget
    this->setCentralWidget(ui->tabWidget);

    ui->tabWidget->removeTab(1); // remove the sub-band tab

    progDataPath = "C:\\A_PE_UPI_Data";
    //Combo boxes
    LoadComboBoxLists();

    //Isntantiate different controllers
    laser       =  new laserController((structScan *)&scanInfo);
    ldv         = new ldvController((structDaq *)&daqInfo);
    daq         = new daqController((structDaq *)&daqInfo,(structScan *)&scanInfo);
    lms         = new lmsController((structLms *)&lmsInfo,(structScan *)&scanInfo);
    filter      = new bandpassController();

    //plots
    qwtPlotOsc = new Plot(ui->widgetOscilloscope,false,"Oscilloscope",(structDaq *)&daqInfo);
    qwtPlotOsc->setObjectName(QStringLiteral("qwtPlotOsc"));
    qwtPlotOsc->setGeometry(QRect(0, 0, 1000, 300));

    qwtPlotResult = new Plot(ui->widgetResultTime,true,NULL,(structDaq *)&daqInfo);
    qwtPlotResult->setObjectName(QStringLiteral("qwtPlotResult"));
    qwtPlotResult->setGeometry(QRect(0, 0, 1300, 210));
    qwtPlotResult->updateGeometry();

    //spects
    qwtSpectrogram = new spectrogram(ui->widgettResultSpect,(structScan *)&scanInfo,0, 0, SPECTBASESIZE+150, SPECTBASESIZE,NULL,1);
    qwtSpectrogram->setObjectName(QStringLiteral("ResultSpectrogram"));


    qwtSpectrogram2 = new spectrogram(ui->widgettResultSpect,(structScan *)&scanInfo,0, 0, SPECTBASESIZE+150, SPECTBASESIZE,NULL,0);
    qwtSpectrogram2->setObjectName(QStringLiteral("ScreenShotSpectrogram"));
/*
    for (int i=0;i<NUMOFBANDS;i++)
    {
        int left = 0,top = 0;
        if (i == 1)
        {
            left = SPECTBASESIZE+150+80;
            top = 0;
        }
        if (i == 2)
        {
            left = 0;
            top = SPECTBASESIZE+50;
        }
        QString name = "Subband - " + QString::number(i+1);

        qwtSpectrogramSubband[i] = new spectrogram(ui->widgetSubbandSpect,(structScan *)&scanInfo,
                                                   left,top,SPECTBASESIZE+150,SPECTBASESIZE,name,(i==0)?(NUMOFBANDS-1):0);

        qwtSpectrogramSubband[i]->setObjectName(name);
        connect(ui->dial_intensity_Subband,SIGNAL(valueChanged(int)),qwtSpectrogramSubband[i],SLOT(setIntensity(int)));
    }

    for (int i=0;i<(NUMOFBANDS-1);i++)
        connect(qwtSpectrogramSubband[i],SIGNAL(placeSlave(int,int)),qwtSpectrogramSubband[i+1],SLOT(placeSlaveslot(int,int)));
*/
    dataProc    = new dataProcessor((structDaq *)&daqInfo,(structScan *)&scanInfo, (structResult *) &resultInfo,
                                    qwtSpectrogram,qwtSpectrogram2,progDataPath,qwtSpectrogramSubband,ui->labelMovieDebug);
    //timers
    mainTimer = new QTimer(this);
    connect(mainTimer, SIGNAL(timeout()), this, SLOT(incrSlider()));

    mainTimerSubband = new QTimer(this);
    connect(mainTimerSubband, SIGNAL(timeout()), this, SLOT(incrSliderSubband()));

    //connections

    //laser
    connect(laser,SIGNAL(updateProgressBar_laserControllerSignal(int)),this,SLOT(updateProgressBar_mainwindowSlot(int)));
    connect(laser,SIGNAL(updateStatusBar_laserControllerSignal(QString)),this,SLOT(updateStatusBar_mainwindowSlot(QString)));

    //dataproc Incoming Signals
    //controllers
    connect(this,SIGNAL(postProcessingVtwamRequired(QString)),dataProc,SLOT(postProcessingVtwamRequested(QString)));
    connect(this,SIGNAL(postProcessingFilteringRequired()),dataProc,SLOT(postProcessingFilteringRequested()));
    connect(daq,SIGNAL(scanFinished(int)),dataProc,SLOT(scanFinished(int)));
    connect(daq,SIGNAL(newWfmReadyForCopy(short *,int,short)),dataProc,SLOT(newWfmCopyToArray_slot(short *,int,short)));
    connect(qwtSpectrogram,SIGNAL(pointToPlot(int)),dataProc,SLOT(plotResultTime(int)));

    //dataproc Outgoing Signals
    connect(dataProc,SIGNAL(updateProgressBar_dataProcessorSignal(int)),this,SLOT(updateProgressBar_mainwindowSlot(int)));
    connect(dataProc,SIGNAL(updateStatusBar_dataProcessorSignal(QString)),this,SLOT(updateStatusBar_mainwindowSlot(QString)));
    connect(dataProc,SIGNAL(updateResultTimePlot(short *, QString)),qwtPlotResult,SLOT(UpdateCurve(short *, QString)));
    connect(dataProc,SIGNAL(wfmCopyDone_sig(int)),daq,SLOT(wfmCopyDone(int)));

    //gui controls
    connect(ui->pushButtonLoadData,SIGNAL(clicked(bool)),dataProc,SLOT(loadData(bool)));
    connect(ui->pushButtonSaveData,SIGNAL(clicked(bool)),dataProc,SLOT(saveData(bool)));

    connect(ui->groupBoxFilter,SIGNAL(toggled(bool)),dataProc,SLOT(selectDisplayBuffer(bool)));
    connect(ui->groupBoxFilter,SIGNAL(toggled(bool)),ui->groupBoxFilterStep1,SLOT(setChecked(bool)));

    connect(ui->horizontalSliderFrame,SIGNAL(valueChanged(int)),dataProc, SLOT(setframeNum(int)));
    //connect(ui->pushButtonCapture, SIGNAL(clicked()),dataProc,SLOT(saveScreenshot()));
    //connect(ui->pushButtonCapture, SIGNAL(clicked()),ui->dial_intensity_2,SLOT(hide()));
    connect(ui->pushButtonSaveMovie, SIGNAL(clicked()),dataProc,SLOT(saveMovie()));


    //daq
    connect(daq,SIGNAL(updateProgressBar_daqControllerSignal(int)),this,SLOT(updateProgressBar_mainwindowSlot(int)));
    connect(daq,SIGNAL(updateStatusBar_daqControllerSignal(QString)),this,SLOT(updateStatusBar_mainwindowSlot(QString)));
    connect(daq,SIGNAL(scanFinished(int )),this,SLOT(scanFinished_main()));
    connect(daq,SIGNAL(updatePlotOsci(short*)),qwtPlotOsc,SLOT(UpdateCurve(short*)));


    //navigator LMS
    connect(ui->pushButtonJogzp,SIGNAL(pressed()),lms, SLOT(lmsMoveyp()));
    connect(ui->pushButtonJogzn,SIGNAL(pressed()),lms, SLOT(lmsMoveyn()));
    connect(ui->pushButtonJogxp,SIGNAL(pressed()),lms, SLOT(lmsMovexp()));
    connect(ui->pushButtonJogxn,SIGNAL(pressed()),lms, SLOT(lmsMovexn()));

    connect(ui->pushButtonOrigin,SIGNAL(clicked(bool)),lms, SLOT(lmsInitialize()));
    connect(ui->lineEditStandofDistance,SIGNAL(editingFinished()),lms, SLOT(lmsInitialize()));
    connect(ui->lineEditScanHeight,SIGNAL(editingFinished()),lms, SLOT(lmsInitialize()));
    connect(ui->lineEditScanWidth,SIGNAL(editingFinished()),lms, SLOT(lmsInitialize()));
    connect(ui->checkBoxDisplayScanArea,SIGNAL(clicked()),lms, SLOT(lmsDisplayArea()));
    connect(ui->pushButtonLmsStop,SIGNAL(pressed()),this, SLOT(Stop()));
    connect(ui->pushButtonLmsStop2,SIGNAL(pressed()),this, SLOT(Stop()));
    //    connect(ui->pushButtonGetpos,SIGNAL(pressed()),lms, SLOT(getPosX()));
    //    connect(ui->pushButtonGetpos,SIGNAL(pressed()),lms, SLOT(getPosZ()));
    connect(lms,SIGNAL(Xpos(QString)),ui->lcdNumberXpos, SLOT(display(QString)));
    connect(lms,SIGNAL(Ypos(QString)),ui->lcdNumberZpos, SLOT(display(QString)));

    //result tab
    connect(ui->dial_intensity,SIGNAL(valueChanged(int)),qwtSpectrogram,SLOT(setIntensity(int)));
    //connect(ui->horizontalSliderFrame,SIGNAL(valueChanged(int)),ui->labelFrame, SLOT(setNum(int)));
    connect(ui->horizontalSliderFrame,SIGNAL(valueChanged(int)),this, SLOT(setlabelFrame(int)));
    connect(ui->horizontalSliderFrame,SIGNAL(valueChanged(int)),qwtPlotResult, SLOT(updateVertMarker(int)));
    connect(ui->horizontalSliderFrame,SIGNAL(mouseMidButton(bool,int)),this, SLOT(updateVtwamInputs(bool,int)));
    //connect(ui->dial_intensity_2,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
    ui->dial_intensity_2->hide();
    ui->labelVtwamRangeTitle->hide();
    connect(ui->dial_intensity,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
    connect(ui->pushButtonPlayPause,SIGNAL(toggled(bool)),this, SLOT(playPauseResult(bool)));
    connect(qwtSpectrogram,SIGNAL(placeSlave(int,int)),qwtSpectrogram2,SLOT(placeSlaveslot(int,int)));

    //subband tab
    connect(ui->horizontalSliderSubband,SIGNAL(valueChanged(int)),dataProc, SLOT(setframeNumSubband(int)));
    connect(ui->horizontalSliderSubband,SIGNAL(valueChanged(int)),ui->labelFrameSubband, SLOT(setNum(int)));
    connect(ui->pushButtonPlayPauseSubband,SIGNAL(toggled(bool)),this, SLOT(playPauseResultSubband(bool)));

    connect(ui->pushButtonCaptureVtwam,SIGNAL(clicked(bool)),dataProc, SLOT(saveScreenshotVtwam()));


    connect(ui->pushButtonSaveSettings,SIGNAL(pressed()),this,SLOT(saveSetting()));
    connect(ui->pushButtonLoadSettings,SIGNAL(pressed()),this,SLOT(loadSetting()));
    connect(ui->checkBoxGreyscale,SIGNAL(toggled(bool)),this->qwtSpectrogram,SLOT(toggleUWPIGreyScale(bool)));
    connect(ui->checkBoxGreyscale,SIGNAL(toggled(bool)),this->qwtSpectrogram2,SLOT(toggleUWPIGreyScale(bool)));

//    connect(dataProc,SIGNAL(setStagePosX(uint,bool)),stage,SLOT(setPosX(uint,bool))); CHK
//    connect(dataProc,SIGNAL(setStagePosZ(uint,bool)),stage,SLOT(setPosZ(uint,bool)));


    connect(ldv,SIGNAL(ldvSignalLevel(int)),ui->ldvSignalLevel,SLOT(setValue(int)));

    connect(ui->enumScanInterval,SIGNAL(currentIndexChanged(int)),this,SLOT(populatePrfList(int)));

    enlargeResultDlgn = new DialogEnlarge(this);
    connect(this->enlargeResultDlgn,SIGNAL(finished(int)),this,SLOT(resizeToNormal()));

    connect(enlargeResultDlgn->giveSlider(),SIGNAL(valueChanged(int)),this->qwtSpectrogram,SLOT(updateAxisXY(int)));
    connect(ui->checkBoxRealTimeMedian,SIGNAL(toggled(bool)),this->dataProc,SLOT(setRlTmMdFlt(bool)));
    connect(this,SIGNAL(destroyed(QObject*)),this->enlargeResultDlgn,SLOT(close()));
    connect(ui->checkBoxBoostSpeed,SIGNAL(toggled(bool)),dataProc,SLOT(setBoostSpeedEn(bool)));
    connect(dataProc,SIGNAL(setFreqMode(short)),this,SLOT(changeFreqMode(short)));

    connect(ui->enumDaqChannels,SIGNAL(currentIndexChanged(int)),this,SLOT(freqModeChanged_slot()));

    InitSettingPars(); // The init settings should be loaded from the last settings used.
    //update the x-axis of the plots
    UpdateSettingsStruct();
    qwtPlotOsc->updateAxisScale();
    qwtPlotResult->updateAxisScale();
    qwtSpectrogram->updateAxisXY();

    isHighRange = false;
    lms->lmsInitialize();

#if ACTUALSYSTEM
    QTimer::singleShot(50,this,SLOT(deviceConnect()));
    //Stop();
//    on_pushButtonFilterConfig_pressed();
#endif

}

MainWindow::~MainWindow()
{
#if ACTUALSYSTEM
    //Stop();
#endif

    //wait for more than a second shoulkd servo stop can be reset in the stage
    //Sleep(3000);

    //delete enlargeResultDlgn;
    delete mainTimer;
    delete mainTimerSubband;
    delete laser;
    delete ldv;
    delete daq;
    delete dataProc;
    delete qwtPlotOsc;
    delete qwtSpectrogram;
    delete ui;
    delete lms;
}

void MainWindow::setSettingStruct()
{
    settingNumber = 0;
    qDebug()<<"here1";
    //0-Broadband
    settingArr[0].ldvRange              = 50;
    settingArr[0].samplingFreq          = 20;
    settingArr[0].chNum                 = 3;
    settingArr[0].trigDelay             = 176;
    settingArr[0].daqVoltage            = 500;

    settingArr[0].filtPar[1].hiPassCut   = 50;
    settingArr[0].filtPar[1].lowPassCut  = 250;
    settingArr[0].filtPar[1].gain        = 25;

    settingArr[0].filtPar[2].hiPassCut   = 250;
    settingArr[0].filtPar[2].lowPassCut  = 1000;
    settingArr[0].filtPar[2].gain        = 25;

    settingArr[0].filtPar[3].hiPassCut   = 1000;
    settingArr[0].filtPar[3].lowPassCut  = 1500;
    settingArr[0].filtPar[3].gain        = 28;


    //1-Narrowband - 1
    settingArr[1].ldvRange              = 10;
    settingArr[1].samplingFreq          = 20;
    settingArr[1].chNum                 = 1;
    settingArr[1].trigDelay             = 304;
    settingArr[1].daqVoltage            = 500;

    settingArr[1].filtPar[1].hiPassCut   = 50;
    settingArr[1].filtPar[1].lowPassCut  = 250;
    settingArr[1].filtPar[1].gain        = 18;

    //2-Narrowband - 2
    settingArr[2].ldvRange              = 20;
    settingArr[2].samplingFreq          = 60;
    settingArr[2].chNum                 = 3;
    settingArr[2].trigDelay             = 560;
    settingArr[2].daqVoltage            = 500;

    settingArr[2].filtPar[1].hiPassCut   = 250;
    settingArr[2].filtPar[1].lowPassCut  = 500;
    settingArr[2].filtPar[1].gain        = 18;

    settingArr[2].filtPar[2].hiPassCut   = 500;
    settingArr[2].filtPar[2].lowPassCut  = 750;
    settingArr[2].filtPar[2].gain        = 18;

    settingArr[2].filtPar[3].hiPassCut   = 750;
    settingArr[2].filtPar[3].lowPassCut  = 1000;
    settingArr[2].filtPar[3].gain        = 18;

    //3-Narrowband - 3
    settingArr[3].ldvRange              = 50;
    settingArr[3].samplingFreq          = 60;
    settingArr[3].chNum                 = 3;
    settingArr[3].trigDelay             = 520;
    settingArr[3].daqVoltage            = 200;

    settingArr[3].filtPar[1].hiPassCut   = 1000;
    settingArr[3].filtPar[1].lowPassCut  = 1160;
    settingArr[3].filtPar[1].gain        = 25;

    settingArr[3].filtPar[2].hiPassCut   = 1160;
    settingArr[3].filtPar[2].lowPassCut  = 1320;
    settingArr[3].filtPar[2].gain        = 25;

    settingArr[3].filtPar[3].hiPassCut   = 1320;
    settingArr[3].filtPar[3].lowPassCut  = 1500;
    settingArr[3].filtPar[3].gain        = 25;

    //4- HighFreq
    settingArr[4].ldvRange              = 200;
    settingArr[4].samplingFreq          = 60;
    settingArr[4].chNum                 = 1;
    settingArr[4].trigDelay             = 264;
    settingArr[4].daqVoltage            = 200;

    settingArr[4].filtPar[1].hiPassCut   = 1500;
    settingArr[4].filtPar[1].lowPassCut  = 2000;
    settingArr[4].filtPar[1].gain        = 22;

    //5-SingleChan-0
    settingArr[5].ldvRange              = 10;
    settingArr[5].samplingFreq          = 20;
    settingArr[5].chNum                 = 1;
    settingArr[5].trigDelay             = 304;
    settingArr[5].daqVoltage            = 500;

    settingArr[5].filtPar[1].hiPassCut   = 50;
    settingArr[5].filtPar[1].lowPassCut  = 250;
    settingArr[5].filtPar[1].gain        = 18;

    //6-SingleChan-1
    settingArr[6].ldvRange              = 20;
    settingArr[6].samplingFreq          = 60;
    settingArr[6].chNum                 = 1;
    settingArr[6].trigDelay             = 576;
    settingArr[6].daqVoltage            = 500;

    settingArr[6].filtPar[1].hiPassCut   = 250;
    settingArr[6].filtPar[1].lowPassCut  = 500;
    settingArr[6].filtPar[1].gain        = 18;

    //7-SingleChan-2
    settingArr[7].ldvRange              = 20;
    settingArr[7].samplingFreq          = 60;
    settingArr[7].chNum                 = 1;
    settingArr[7].trigDelay             = 568;
    settingArr[7].daqVoltage            = 500;

    settingArr[7].filtPar[1].hiPassCut   = 500;
    settingArr[7].filtPar[1].lowPassCut  = 750;
    settingArr[7].filtPar[1].gain        = 18;

    //8-SingleChan-3
    settingArr[8].ldvRange              = 20;
    settingArr[8].samplingFreq          = 60;
    settingArr[8].chNum                 = 1;
    settingArr[8].trigDelay             = 560;
    settingArr[8].daqVoltage            = 500;

    settingArr[8].filtPar[1].hiPassCut   = 750;
    settingArr[8].filtPar[1].lowPassCut  = 1000;
    settingArr[8].filtPar[1].gain        = 18;

    //9-SingleChan-4
    settingArr[9].ldvRange               = 50;
    settingArr[9].samplingFreq           = 60;
    settingArr[9].chNum                  = 1;
    settingArr[9].trigDelay             = 536;
    settingArr[9].daqVoltage            = 200;

    settingArr[9].filtPar[1].hiPassCut   = 1000;
    settingArr[9].filtPar[1].lowPassCut  = 1166;
    settingArr[9].filtPar[1].gain        = 25;

    //10-SingleChan-5
    settingArr[10].ldvRange               = 50;
    settingArr[10].samplingFreq           = 60;
    settingArr[10].chNum                  = 1;
    settingArr[10].trigDelay             = 528;
    settingArr[10].daqVoltage            = 200;

    settingArr[10].filtPar[1].hiPassCut   = 1166;
    settingArr[10].filtPar[1].lowPassCut  = 1332;
    settingArr[10].filtPar[1].gain        = 25;

    //11-SingleChan-6
    settingArr[11].ldvRange               = 50;
    settingArr[11].samplingFreq           = 60;
    settingArr[11].chNum                  = 1;
    settingArr[11].trigDelay             = 520;
    settingArr[11].daqVoltage            = 200;

    settingArr[11].filtPar[1].hiPassCut   = 1332;
    settingArr[11].filtPar[1].lowPassCut  = 1500;
    settingArr[11].filtPar[1].gain        = 25;
}



void MainWindow::saveSetting(bool defaultFile)
{
    QFile myfileout;

    QString captureStoragePath = progDataPath + "\\Setting\\";
    QString captureFileName;

    if (defaultFile)
    {
        captureFileName = "LastSetting.bin";
    }
    else
    {
        QDateTime now = QDateTime::currentDateTime();
        captureFileName = "Setting"+now.toString("ddMMyy_hhmmss")+".bin";
    }

    if( QDir(captureStoragePath).exists() == false)
        QDir().mkpath(captureStoragePath);

    myfileout.setFileName(captureStoragePath+captureFileName);

    if(!myfileout.open(QIODevice::WriteOnly))
    {
        QMessageBox errBox;
        errBox.setText("Could not open file for writing.");
        errBox.exec();
        return;
    }
    QDataStream out(&myfileout);
    out.setByteOrder(QDataStream::LittleEndian);


    //Scan
    out<<ui->lineEditScanWidth->text();
    out<<ui->lineEditScanHeight->text();
    out<<ui->enumScanInterval->currentIndex();
    out<<ui->enumTotalScans->currentIndex();

    out<<ui->enumPRF->currentIndex();
    out<<ui->enumCurrent->currentIndex();


    //Navigator
    //press the get pos button
    ui->pushButtonGetpos->pressed();
    out<<QString::number(ui->lcdNumberXpos->value());
    out<<QString::number(ui->lcdNumberZpos->value());
/*
    out<<stage->scanStartPosX;
    out<<stage->scanStartPosZ;
*/
    //Data Acquisition
    //out<<ui->radioButtonLowFreq->isChecked();
    //out<<ui->radioButtonMidFreq->isChecked();
    //out<<ui->radioButtonHighFreq->isChecked();

    myfileout.close();

    if (defaultFile == false)
    {
        QMessageBox msgBox(QMessageBox::Information, tr("Scan Setting"),tr("Successfully saved settings in \n")+captureStoragePath+captureFileName, 0, this);
        msgBox.exec();
        return;
    }
}

bool MainWindow::loadSetting(bool defaultFile)
{
    QString FileName;
    QFile myfilein;
    int intTemp;
    float floatTemp;
    bool boolTemp;
    QString stringTemp;

    if (defaultFile)
    {
        FileName = progDataPath + "\\Setting\\LastSetting.bin";
    }
    else
    {
        QFileDialog *fd = new QFileDialog;
        //QTreeView *tree = fd->findChild <QTreeView*>();
        //tree->setRootIsDecorated(true);
        //tree->setItemsExpandable(true);
        fd->setFileMode(QFileDialog::ExistingFile);
        fd->setViewMode(QFileDialog::Detail);
        fd->setDirectory(progDataPath+"\\Setting");
        if (fd->exec())
        {
            FileName = fd->selectedFiles()[0];
            qDebug()<<FileName;
        }
        else
        {
            qDebug()<<"Can't open the Setting file.";
            return false;
        }
    }

    myfilein.setFileName(FileName);

    if(!myfilein.open(QIODevice::ReadOnly))
    {
        //QMessageBox msgBox(QMessageBox::Critical, tr("File Error"),tr("Could'nt open parameter file for data."), 0);
        //msgBox.exec();
        qDebug()<<"Can't open the Setting file.";
        return false;
    }
    QDataStream in(&myfilein);
    in.setByteOrder(QDataStream::LittleEndian);


    //Scan
    in>>stringTemp;
    ui->lineEditScanWidth->setText(stringTemp);
    in>>stringTemp;
    ui->lineEditScanHeight->setText(stringTemp);
    in>>intTemp;
    ui->enumScanInterval->setCurrentIndex(intTemp);
    in>>intTemp;
    ui->enumTotalScans->setCurrentIndex(intTemp);

    in>>intTemp;
    ui->enumPRF->setCurrentIndex(intTemp);
    in>>intTemp;
    ui->enumCurrent->setCurrentIndex(intTemp);

    //Navigator
    //press the get pos button
    //ui->pushButtonGetpos->pressed();


    in>>stringTemp;
    ui->lineEditXpos->setText(stringTemp);
    in>>stringTemp;
    ui->lineEditZPos->setText(stringTemp);

    /*
    in>>floatTemp;
    ui->lineEditXpos->setText(QString::number(floatTemp/1000));
    in>>floatTemp;
    ui->lineEditZPos->setText(QString::number(floatTemp/1000));
    */

    //Data Acquisition
    //in>>boolTemp;
    //ui->radioButtonLowFreq->setChecked(boolTemp);
    //in>>boolTemp;
    //ui->radioButtonMidFreq->setChecked(boolTemp);
    //in>>boolTemp;
    //ui->radioButtonHighFreq->setChecked(boolTemp);

    myfilein.close();

    if (defaultFile == false)
    {
        QMessageBox msgBox(QMessageBox::Information, tr("Scan Setting"),tr("Successfully loaded settings from \n ")+FileName, 0, this);
        msgBox.exec();
    }
    return true;
}

void MainWindow::setlabelFrame(int frameNumber)
{

    QString microSec = " \xC2\xB5s";
    QString labelString = "0"+microSec;
    if (daqInfo.SamplingFreq != 0)
        labelString = QString::number(((double)frameNumber/daqInfo.SamplingFreq) ,'g',3)+microSec;
    ui->labelFrame->setText(labelString);
}
void MainWindow::deviceConnect()
{
    QString ldvPortName;
    QString laserComPort;
    QList<QSerialPortInfo> serialPortInfo = QSerialPortInfo::availablePorts();
    int comPortNum;
    //if ((comPortNum = serialPortInfo.size()) > 3)
    //{
    //    QMessageBox msgBox(QMessageBox::Critical, tr("Connection Error"),tr("More than expected COM ports. Please only connect LDV and laser controller."), 0, this);
    //    msgBox.exec();
    //    return;
    //}

    if (laser->srchPortandConnect("COM8",&laserComPort) == true)
    {
        foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
        {
            if (info.portName() != "COM1" && info.portName() != laserComPort )
            {
                ldvPortName = info.portName();
                if (ldv->connectLdv(ldvPortName) == false)
                {//make an error
                    QMessageBox msgBox(QMessageBox::Critical, tr("Connection Error"),tr("LDV not found. Please reconnect and try again."), 0, this);
                    msgBox.exec();
                    return;
                }
                else
                {
                    return;
                }
            }
        }
    }
    else
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Connection Error"),tr("Laser Controller not found. Please reconnect and try again."), 0, this);
        msgBox.exec();
        if (ldv->connectLdv("COM3") == false)
        {
            QMessageBox msgBox(QMessageBox::Critical, tr("Connection Error"),tr("LDV not found.Please reconnect and try again."), 0, this);
            msgBox.exec();
        }
        return;
    }
}

void MainWindow::updateStatusBar_mainwindowSlot(QString StatusTip)
{
    ui->statusBar->showMessage(StatusTip);
}

void MainWindow::updateProgressBar_mainwindowSlot(int Progress)
{
    ui->progressBar->show();
    ui->progressBar->setValue(Progress);
    if (Progress == 100)
    {
        QTimer::singleShot( 500, this, SLOT(endProgressBar_mainwindowSlot() ));
    }
}

void MainWindow::endProgressBar_mainwindowSlot()
{
    ui->progressBar->hide();
}

void MainWindow::populatePrfList(int intervalIndex)
{

    QString sPRF;
    ui->enumPRF->clear();

//    ui->enumPRF->addItem("0.001");
//    ui->enumPRF->addItem("0.002");
//    ui->enumPRF->addItem("0.005");
//    ui->enumPRF->addItem("0.01");
//    ui->enumPRF->addItem("0.02");
//    ui->enumPRF->addItem("0.05");
//    ui->enumPRF->addItem("0.1");
//    ui->enumPRF->addItem("0.2");
    ui->enumPRF->addItem("0.5");
    for (int i = 1; i < 11; i++)
    {
        sPRF = QString::number(i);
        ui->enumPRF->addItem(sPRF);
    }
    ui->enumPRF->addItem("20");
//    ui->enumPRF->addItem("50");

    ui->enumPRF->setCurrentIndex(5);

//    ui->enumPRF->clear();
//    if (intervalIndex == 0) //0.05
//    {
//        ui->enumPRF->addItem("1000");
//        ui->enumPRF->addItem("1500");
//        ui->enumPRF->addItem("2000");
//        ui->enumPRF->addItem("2500");
//    }
//    if (intervalIndex == 1) //0.10
//    {
//        ui->enumPRF->addItem("1000");
//        ui->enumPRF->addItem("1500");
//        ui->enumPRF->addItem("2000");
//        ui->enumPRF->addItem("2500");
//    }
//    else if (intervalIndex == 2) //0.25
//    {
//        ui->enumPRF->addItem("600");
//        ui->enumPRF->addItem("800");
//        ui->enumPRF->addItem("1000");
//        ui->enumPRF->addItem("1200");
//        ui->enumPRF->addItem("1400");
//        ui->enumPRF->addItem("1600");
//        ui->enumPRF->addItem("1800");
//        ui->enumPRF->addItem("2000");
//    }
//    else if (intervalIndex == 3) //0.45
//    {
//        ui->enumPRF->addItem("1000");
//    }
//    else if(intervalIndex == 4) //0.50
//    {
//        ui->enumPRF->addItem("300");
//        ui->enumPRF->addItem("400");
//        ui->enumPRF->addItem("500");
//    }
//    else if (intervalIndex == 5) //1.00
//    {
//        ui->enumPRF->addItem("150");
//        ui->enumPRF->addItem("250");
//        ui->enumPRF->addItem("450");
//    }
//    else if (intervalIndex == 6) //3.00
//    {
//        ui->enumPRF->addItem("50");
//        ui->enumPRF->addItem("100");
//        ui->enumPRF->addItem("150");
//    }

//    ui->enumPRF->setCurrentIndex(0);
}

void MainWindow::LoadComboBoxLists()
{
    QStringList listScanInterval,listLaserPRF,listLaserCurrent,listPlaySpeed,listMaterials,
            listTotalScans,listRef;
    //scan
    ui->enumScanInterval->addItems(comboScanInterval(listScanInterval));
    //laser

    ui->enumCurrent->addItems(comboLaserCurrent(listLaserCurrent));

    ui->enumPlaySpeed->addItems(comboPlaySpeed(listPlaySpeed));

    ui->enumImageFilterIterations->addItem("1");
    ui->enumImageFilterIterations->addItem("2");

    ui->enumImageFilterType->addItem("Median");
    ui->enumImageFilterType->addItem("Spatial Avg");

    ui->enumImageFilterSize->addItem("3x3");
    ui->enumImageFilterSize->addItem("5x5");
    ui->enumImageFilterSize->addItem("7x7");

    ui->enumImageFilterIterations_2->addItem("1");
    ui->enumImageFilterIterations_2->addItem("2");

    ui->enumImageFilterType_2->addItem("Median");
    ui->enumImageFilterType_2->addItem("Spatial Avg");

    ui->enumImageFilterSize_2->addItem("3x3");
    ui->enumImageFilterSize_2->addItem("5x5");
    ui->enumImageFilterSize_2->addItem("7x7");

    ui->enumPlaySpeedSubband->addItems(comboPlaySpeed(listPlaySpeed));

    ui->enumTotalScans->addItems(comboTotalScans(listTotalScans));

    populatePrfList(ui->enumScanInterval->currentIndex());

    ui->enumDaqChannels->addItem("CH-0");
    ui->enumDaqChannels->addItem("CH-1");
    ui->enumDaqChannels->addItem("Avg");
}

void MainWindow::InitSettingPars()
{
    //toggle to load all the enums
    ui->checkBoxEnableMultiBand->setChecked(1);
    ui->checkBoxEnableMultiBand->setChecked(0);

    //laser
    if (loadSetting(true)==false)
    { //file for the last setting does not exist so set to default initial values
        ui->enumPRF->setCurrentIndex(6);
        ui->enumCurrent->setCurrentIndex(0);
        ui->lineEditScanWidth->setText("100");
        ui->lineEditScanHeight->setText("100");
        ui->enumScanInterval->setCurrentIndex(1);
    }

    ui->labelLaserStatus->setText("Laser Beam: OFF");
    ui->pushButtonLaserControl->setEnabled(false);
    ui->pushButtonLaserControl->setText("Activate");
    ui->pushButtonLaserControl->setStatusTip("Turn-on the laser beam.");

    ui->dial_intensity_2->setMinimum(1000);
    ui->dial_intensity_2->setMaximum((2<<20)-1);
    ui->dial_intensity_2->setValue(140000);

    ui->dial_intensity->setMinimum(1000);
    ui->dial_intensity->setMaximum((1<<15)-1);
    ui->dial_intensity->setValue(5000);

    ui->horizontalSliderFrame->setMinimum(0);
    ui->horizontalSliderFrame->setMaximum(SAMPLESPERPOINT-1);
    ui->horizontalSliderFrame->setValue(0);

    ui->enumPlaySpeed->setCurrentIndex(1);

    ui->groupBoxFilter->setChecked(false);
    ui->lineEditVtwamStart->setText("0");
    ui->lineEditVtwamEnd->setText("0");

    ui->dial_intensity_Subband->setMinimum(1000);
    ui->dial_intensity_Subband->setMaximum(32768);
    ui->dial_intensity_Subband->setValue(16384);

    ui->horizontalSliderSubband->setMinimum(0);
    ui->horizontalSliderSubband->setMaximum(SAMPLESPERPOINT-1);
    ui->horizontalSliderSubband->setValue(0);

    ui->enumPlaySpeedSubband->setCurrentIndex(1);
    //make sure to toggle to hit the toggle slot
    //ui->radioButtonLowFreq->setChecked(true);
    //ui->radioButtonHighFreq->setChecked(false);
    //ui->groupBoxFilter->setChecked(true);

    ui->groupBoxFilterStep1->setChecked(false);
    ui->groupBoxFilterStep2->setChecked(false);

    QPalette* palRedText = new QPalette();
    palRedText->setColor(QPalette::ButtonText, Qt::red);

    ui->pushButtonLmsStop->setPalette(*palRedText);
    ui->pushButtonLmsStop2->setPalette(*palRedText);

    ui->pushButtonDaqSet->setEnabled(true);
    ui->enumDaqChannels->setCurrentIndex(0);

    //ui->enumMaterials->setCurrentIndex(1);

    ui->lineEditDaqVoltage->setText("500");
    ui->lineEditDaqTrigDelay->setText("64");

    UpdateSettingsStruct();

    ui->labelVtwamStart->setText(" \xC2\xB5s");
    ui->labelVtwamEnd->setText(" \xC2\xB5s");

    resultInfo.vtwamRangeNo = -1;
}

void MainWindow::updateResultParStruct()
{
    resultInfo.filtPass1en          = ui->groupBoxFilterStep1->isChecked();
    resultInfo.filterType           = ui->enumImageFilterType->currentIndex()+1;
    resultInfo.filterRadius         = ui->enumImageFilterSize->currentIndex()+1;
    resultInfo.filterItr            = ui->enumImageFilterIterations->currentIndex()+1;

    resultInfo.filtPass2en           = ui->groupBoxFilterStep2->isChecked();
    resultInfo.filterType2           = ui->enumImageFilterType_2->currentIndex()+1;
    resultInfo.filterRadius2         = ui->enumImageFilterSize_2->currentIndex()+1;
    resultInfo.filterItr2            = ui->enumImageFilterIterations_2->currentIndex()+1;

    //resultInfo.vtwamStartFr          = ui->lineEditVtwamStart->text().toInt();
    //resultInfo.vtwamEndFr            = ui->lineEditVtwamEnd->text().toInt();

    if(resultInfo.vtwamRangeNo > -1)
    {
        resultInfo.vtwamStartFr[resultInfo.vtwamRangeNo]          = qRound(daqInfo.SamplingFreq * ui->lineEditVtwamStart->text().toFloat());
        resultInfo.vtwamEndFr[resultInfo.vtwamRangeNo]            = qRound(daqInfo.SamplingFreq * ui->lineEditVtwamEnd->text().toFloat());
    }
}

void MainWindow::UpdateScanParStruct()
{
    scanInfo.scanHeight         = ui->lineEditScanHeight->text().toFloat();
    scanInfo.scanWidth          = ui->lineEditScanWidth->text().toFloat();
    scanInfo.scanInterval       = ui->enumScanInterval->currentText().toFloat();
    scanInfo.Current            = ui->enumCurrent->currentText();
    scanInfo.PRF                = ui->enumPRF->currentText();
    scanInfo.scansPerInspection = ui->enumTotalScans->currentText().toInt();
    scanInfo.useCurrentResults  = ui->checkBox_useCurrentResults->isChecked();
    //scanInfo.enableTT           = true; //ui->checkBoxTT->isChecked();
}

void MainWindow::UpdateDaqParStruct()
{
    daqInfo.freqMode = 0;
    daqInfo.SamplingFreq    = settingArr[settingNumber].samplingFreq;
    daqInfo.Range           = settingArr[settingNumber].ldvRange;

    daqInfo.ScanPoints              = ((scanInfo.scanHeight/scanInfo.scanInterval)+1)*((scanInfo.scanWidth/scanInfo.scanInterval)+1);
    daqInfo.subbandDecomp           = false;
    daqInfo.ButterWorthIIREnabled   = true;
    daqInfo.totalNumOfScans         = (ui->enumTotalScans->currentText()).toInt();
    daqInfo.chMap   =   ui->enumDaqChannels->currentIndex()+1;

//    daqInfo.daqTrigDelay = ui->lineEditDaqTrigDelay->text().toInt();
//    daqInfo.daqVoltage = ui->lineEditDaqVoltage->text().toInt();
    daqInfo.daqTrigDelay    =   settingArr[settingNumber].trigDelay; //trig
    daqInfo.daqVoltage      = settingArr[settingNumber].daqVoltage;
    qwtPlotOsc->updateAxisScale();
    qwtPlotResult->updateAxisScale();
    qwtSpectrogram->updateAxisXY();

}

void MainWindow::UpdateSettingsStruct()
{
    UpdateScanParStruct();
    UpdateDaqParStruct();
    UpdateLmsParStruct();
}

void MainWindow::UpdateLmsParStruct()
{
    lmsInfo.SOD = ui->lineEditStandofDistance->text();
    lmsInfo.laserStep = ui->spinBoxLaserStep->text().toInt();
}

void MainWindow::on_pushButtonLaserControl_toggled(bool checked)
{
    if (checked)
    {
        //ui->statusBar->showMessage("Activating Laser.");
        if(laser->onSHT())
        {
            QString outText;
            ui->pushButtonLaserControl->setText("Deactivate");
            ui->pushButtonLaserControl->setStatusTip("Deactivate the laser beam.");
            outText = "Laser Beam: ON (" + ui->enumCurrent->currentText() + "%)";
            ui->labelLaserStatus->setText(outText );
        }
        else
        {
            laserError();
        }
    }
    else
    {
        //ui->statusBar->showMessage("Deactivating Laser.");
        if(laser->offSHT() )
        {
            ui->labelLaserStatus->setText("Laser Beam: OFF");
            ui->pushButtonLaserControl->setText("Activate");
            ui->pushButtonLaserControl->setStatusTip("Activate the laser beam.");
        }
        else
        {
            laserError();
        }
    }
}

void MainWindow::hiLaserPwrWarning()
{

    QString sCurrent;
    laser->getCurrent(&sCurrent);

    sCurrent.toFloat()/10;
//    if ((sCurrent.toFloat()/10)>18.0)
//    {
//        QMessageBox msgBox(QMessageBox::Warning, tr("Laser Power Warning"),tr("High laser power!!!"), 0, this);
//        msgBox.exec();
//    }
}

void MainWindow::on_pushButtonLaserConfigPrfCurr_clicked()
{
    UpdateScanParStruct();
    if (laser->initLaserControllerDone == false)
    {// if init was not done then paras will be set as part of init.
        ui->statusBar->showMessage("Initializing the laser and setting parameters.");
        if(laser->initLaserController() == false)
        {
            laserError();
        }
        else
        {
            ui->statusBar->showMessage("Laser has been configured.");
            ui->pushButtonLaserControl->setEnabled(true);
            hiLaserPwrWarning();
        }
    }
    else
    {//set current and PRF separately.
        ui->statusBar->showMessage("Setting laser parameters.");
        if(!( laser->setPRF() && laser->setCurrent() ))
        {
            laserError();
        }
        else
        {
            ui->statusBar->showMessage("Laser has been configured.");
            ui->pushButtonLaserControl->setEnabled(true);
            hiLaserPwrWarning();

        }
    }
}

void MainWindow::laserError()
{
    QMessageBox msgBox(QMessageBox::Critical, tr("Connection Error"),tr("Laser controller not found.Please reconnect and try again."), 0, this);
    msgBox.exec();

    //The laser may have been turned off so make sure to initialize again next time.
    laser->initLaserControllerDone = true;
    ui->pushButtonLaserControl->setEnabled(false);

    ui->labelLaserStatus->setText("Laser Beam: OFF");
    ui->pushButtonLaserControl->setText("Activate");
    ui->pushButtonLaserControl->setStatusTip("Activate the laser beam.");
}

void MainWindow::on_pushButtonLdvAutoFocus_clicked()
{
    if ( ! (ldv->setAutoFocus()) )
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Connection Error"),tr("LDV not found.Please reconnect and try again."), 0, this);
        msgBox.exec();
    }
    else
    {
        ui->statusBar->showMessage("Ldv has been configured.");
    }
}

void MainWindow::setLdvRange()
{
    bool success;
    success = ldv->setRange();


    if ( !(success) )
    {
    //    QMessageBox msgBox(QMessageBox::Critical, tr("Connection Error"),tr("LDV not found.Please reconnect and try again."), 0, this);
      //  msgBox.exec();
    }
    else
    {
        ui->statusBar->showMessage("Ldv has been configured.");
    }
}

void MainWindow::on_lineEditScanHeight_editingFinished()
{
    ui->lineEditScanHeight->setText(QString::number(boundScanPars(ui->lineEditScanHeight->text().toFloat())));
}

float MainWindow::boundScanPars(float enteredPar)
{
    /*
    int validPar;

    if(enteredPar >= 5)
    {
        if(enteredPar <= 1500)
        {
            if (enteredPar%10 == 0)
                validPar = enteredPar;
            else
                validPar = (10-enteredPar%10) +  enteredPar;
        }
        else
           validPar = 1500;
    }
    else
        validPar = 5;

    return validPar;
    */
    return enteredPar;
}

void MainWindow::on_lineEditScanWidth_editingFinished()
{
    ui->lineEditScanWidth->setText(QString::number(boundScanPars(ui->lineEditScanWidth->text().toFloat())));
}


void MainWindow::on_pushButtonSetpos_released()
{
    lms->lmsSetPos(ui->lineEditXpos->text().toInt(),ui->lineEditZPos->text().toInt());
}

void MainWindow::on_lineEditXpos_editingFinished()
{
    double val;
    val = ui->lineEditXpos->text().toDouble();
    //ui->lineEditXpos->setText(QString::number(val,'f',2));
}

void MainWindow::on_lineEditZPos_editingFinished()
{
    double val;
    val = ui->lineEditZPos->text().toDouble();
    //ui->lineEditZPos->setText(QString::number(val,'f',2));
}


void MainWindow::initDoneMsgBox()
{
    QMessageBox msgBox(QMessageBox::Information, tr("Scan initialization"),tr("Scan initialization done."), 0, this);
    msgBox.exec();
}

void MainWindow::Stop()
{
    stopPressed = true;
    scanInfo.quitScan= true;


    if (daq->OsciModeEn==false)
        daq->StopAcquisition();
    dataProc->stop();

    on_pushButtonLaserControl_toggled(false);//turn on SHT
    laser->offEXT();
//    connect(dataProc,SIGNAL(setStagePosX(uint,bool)),stage,SLOT(setPosX(uint,bool))); CHK
//    connect(dataProc,SIGNAL(setStagePosZ(uint,bool)),stage,SLOT(setPosZ(uint,bool)));
    //daq->Configure(true);//goto oscilo mode
}

void MainWindow::settingBeforeNewScan()
{
    connect(ui->dial_intensity,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
    disconnect(ui->dial_intensity_2,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
    ui->dial_intensity_2->hide();
    emit ui->dial_intensity->valueChanged(ui->dial_intensity->value()+1);
    emit ui->dial_intensity->valueChanged(ui->dial_intensity->value()-1);

    // reset the result - filter - group box
    ui->enumImageFilterIterations->setCurrentIndex(0);
    ui->enumImageFilterType->setCurrentIndex(0);
    ui->enumImageFilterSize->setCurrentIndex(0);

    ui->enumImageFilterIterations_2->setCurrentIndex(0);
    ui->enumImageFilterType_2->setCurrentIndex(0);
    ui->enumImageFilterSize_2->setCurrentIndex(0);

    ui->groupBoxFilterStep1->setChecked(false);
    ui->groupBoxFilterStep2->setChecked(false);

    ui->groupBoxFilter->setChecked(false);


    // reset the result - time - plot
    this->qwtPlotResult->initPlot();
    this->ui->horizontalSliderFrame->setValue(0);
}

void MainWindow::on_pushButtonDaqSet_clicked()
{
    if (ui->checkBox_useCurrentResults->isChecked())
    {//load all the pars of last scan from file to make sure these settings are all same

    }
    qDebug()<<"\nMainWindow::on_pushButtonDaqSet_clicked *******Starting a new inspection********";
    on_lineEditScanHeight_editingFinished();
    on_lineEditScanWidth_editingFinished();
    UpdateSettingsStruct();
    scansDone = 0;
    stopPressed = false;

    on_pushButtonFilterConfig_pressed();
    if(dataProc->allocateMem())
    {
        settingBeforeNewScan();
        saveSetting(true);
#if ACTUALSYSTEM

//        disconnect(dataProc,SIGNAL(setStagePosX(uint,bool)),stage,SLOT(setPosX(uint,bool))); CHK
//        disconnect(dataProc,SIGNAL(setStagePosZ(uint,bool)),stage,SLOT(setPosZ(uint,bool)));

//        if (ui->checkBox_useCurrentResults->isChecked() == true)
//        {
//            stage->gotoScanStart();
//            while(stage->isReadyForRptScan() == false)
//                QCoreApplication::processEvents();
//        }

        scanInfo.quitScan=false;
        laser->onEXT();
        laser->onSHT();
        daq->Configure(false);
        setLdvRange();
        QThread* scanThread = new QThread;
        lmsController* lmsScan = new lmsController((structLms *)&lmsInfo,(structScan *)&scanInfo);
        lmsScan->moveToThread(scanThread);
        connect(scanThread, SIGNAL (started()), lmsScan, SLOT (lmsScan()));
        connect(lmsScan, SIGNAL (finished()), scanThread, SLOT (quit()));
        connect(lmsScan, SIGNAL (finished()), lmsScan, SLOT (deleteLater()));
        connect(this, SIGNAL (endScanThread()), scanThread, SLOT (quit()));
        connect(this, SIGNAL (endScanThread()), lmsScan, SLOT (deleteLater()));
        connect(scanThread, SIGNAL (finished()), scanThread, SLOT (deleteLater()));
        scanThread->start();

#else
        daq->Configure(false);
        /*
       //dataProc->scanFinished(100);
       //emit daq->scanFinished(0);
       //dataProc->setframeNum(0);
       */
#endif
        ui->tabWidget->setCurrentIndex(1);
    }
    else
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Error: Out Of Memory"),tr("Reduce the number of scan points and try again."), 0, this);
        msgBox.exec();
    }
}

void MainWindow::scanFinished_main()
{
    qDebug()<<"MainWindow::scanFinished_main *******Scan finished********\n";
    //go to start position again
    //laser->offSHT();
    ui->checkBox_useCurrentResults->setEnabled(true);
    scansDone++;
#if ACTUALSYSTEM
    on_pushButtonLaserControl_toggled(false);//turn on SHT
    laser->offEXT();  
#endif

    if (scanInfo.scansPerInspection == 1)
        daq->Configure(true);//goto oscilo mode

    // more scans needed
     if (scanInfo.scansPerInspection > 1 && stopPressed == false )
     {
         if (scansDone < scanInfo.scansPerInspection)
         {
             qDebug()<<"MainWindow::on_pushButtonDaqSet_clicked *******Starting a new scan. scansDone:"<<scansDone<<
                       "scanInfo.scansPerInspection"<<scanInfo.scansPerInspection;
             //issue another scan
             //wait for 5 seconds in between the respective scans to allow for stage to return to original position
             /*
             {
                 QElapsedTimer timer;
                 timer.start();
                 while(timer.elapsed()<15000)
                     QCoreApplication::processEvents();
            }*/

//             while(stage->isReadyForRptScan() == false) CHK
                 QCoreApplication::processEvents();

            #if ACTUALSYSTEM
                    laser->onEXT();
                    daq->Configure(false);
                    setLdvRange();//ldv->setRange();
                    on_pushButtonLaserControl_toggled(true);//turn on SHT
//                    stage->startScan((unsigned int)scanInfo.scanWidth, CHK
//                                     (unsigned int)scanInfo.scanHeight,
//                                     (unsigned int )scanInfo.PRF.toUInt(),
//                                     (float)scanInfo.scanInterval,
//                                     false);
                    ui->tabWidget->setCurrentIndex(1);
            #else
                    //emit daq->scanFinished(0);

                    //daq->Configure(false);
            #endif

         }
         else
         {
             // trigger rpt scan finished in data processor.
             dataProc->rptScanFinished();
             daq->Configure(true);//goto oscilo mode
//             connect(dataProc,SIGNAL(setStagePosX(uint,bool)),stage,SLOT(setPosX(uint,bool))); CHK
//             connect(dataProc,SIGNAL(setStagePosZ(uint,bool)),stage,SLOT(setPosZ(uint,bool)));
         }
     }
     else if (scanInfo.useCurrentResults)
     {
         dataProc->rptScanFinished();
         daq->Configure(true);//goto oscilo mode
//         connect(dataProc,SIGNAL(setStagePosX(uint,bool)),stage,SLOT(setPosX(uint,bool))); CHK
//         connect(dataProc,SIGNAL(setStagePosZ(uint,bool)),stage,SLOT(setPosZ(uint,bool)));
     }
     else
     {
//         connect(dataProc,SIGNAL(setStagePosX(uint,bool)),stage,SLOT(setPosX(uint,bool))); CHK
//         connect(dataProc,SIGNAL(setStagePosZ(uint,bool)),stage,SLOT(setPosZ(uint,bool)));
     }
}

void MainWindow::on_pushButtonQuit_clicked()
{
    int result;
    QMessageBox msgBox(QMessageBox::Question, tr("Exit"),tr("Do you want to end this session ?"),
                       QMessageBox::Yes|QMessageBox::No);
    result = msgBox.exec();

    if (result ==QMessageBox::No)
    return;

    qDebug()<<"Now turnoff everything.";
    updateStatusBar_mainwindowSlot("Preparing for system turn-off.");

#ifdef ACTUALSYSTEM
    //1-stop the stage.
    Stop();

    {
        QElapsedTimer timer;
        timer.start();
        while(timer.elapsed()<100)
            QCoreApplication::processEvents();
   }

//    stage->clearServoStop(); CHK


//    stage->setPosX(0); //CHK
//    stage->setPosZ(0);


    //stage->clearServoStop();
    //2-take stage to desired position.


    //3-set laser all off.
    laser->offDIO();
    laser->offQS();
#endif

    QCoreApplication::quit();
}

void MainWindow::on_actionAL_CFRP_triggered()
{
    QLabel* help=new QLabel();
    help->setWindowTitle("Aluminium/Graphite Frequency Table");
    help->setWindowFlags(Qt::Tool); //or Qt::Tool, Qt::Dialog if you like
    help->setPixmap(QPixmap("://images/Band-pass-filter-range.gif"));
    help->show();
}

void MainWindow::on_actionKAF_Standard_triggered()
{
    QLabel* help=new QLabel();
    help->setWindowTitle("KAF Standard Frequency Table");
    help->setWindowFlags(Qt::Tool); //or Qt::Tool, Qt::Dialog if you like
    help->setPixmap(QPixmap("://images/KAFrefSlide.GIF"));
    help->show();
}

void MainWindow::on_actionAbout_triggered()
{
    //QMessageBox msgBox(QMessageBox::about, tr("Memory Error"),tr("dataProcessor::scanFinished - wfmPtr = NULL"));
    QMessageBox::about(this, trUtf8("About"), trUtf8("Angular Scan Pulse Echo Ultrasonic Propogation Imaging System (ver1.0) \n "
                                                     "by Opto-Electro-Structural-Lab KAIST"));
    //msgBox.exec();
    return;
}

void MainWindow::on_actionPulse_Energy_Table_triggered()
{
    QLabel* help=new QLabel();
    help->setWindowTitle("KAF Standard Frequency Table");
    help->setWindowFlags(Qt::Tool); //or Qt::Tool, Qt::Dialog if you like
    help->setPixmap(QPixmap("://images/PulseEnergyTable.GIF"));
    help->show();
}

void MainWindow::on_pushButtonSaveSettings_2_pressed()
{
    UpdateDaqParStruct();
    daq->Configure(true);
}
//-------------------------------------------------Result tab----------------------------------------
#define SPEEDX1INTERVALMS 100
//Spectrogram takes 23~25ms to render. Using fastes 30ms per update to give breathing room to worker threads
//x1->100ms+10ms(overhead)
//x2->50+10ms(overhead)
//x4->25+10ms(overhead)
//anything beyond this will just choke up the timer thread Q since the processing is bottlenecked by the spectrogram update
void MainWindow::playPauseResult(bool play)
{
    sliderIncVal = 1;
    if (play)
    {
       incrSlider();
       if (ui->enumPlaySpeed->currentIndex()<4) // only speed up till X4
       {
           mainTimer->start(SPEEDX1INTERVALMS /(1 << ui->enumPlaySpeed->currentIndex()) );
           sliderIncVal = 1;
       }
       else
       {
           mainTimer->start(SPEEDX1INTERVALMS/4 );
           sliderIncVal = ui->enumPlaySpeed->currentIndex() - 3;
       }


       ui->pushButtonPlayPause->setText("Pause");
       ui->pushButtonPlayPause->setStatusTip("Pause the automatic display of data.");
       ui->pushButtonPlayPause->setChecked(true);
    }
    else
    {
        mainTimer->stop();
        ui->pushButtonPlayPause->setText("Play");
        ui->pushButtonPlayPause->setChecked(false);
        ui->pushButtonPlayPause->setStatusTip("Automatically traverse the data in a frame-wise fashion. ");
    }
}

void MainWindow::incrSlider()
{
    int curVal = ui->horizontalSliderFrame->value();
    curVal += sliderIncVal;
    if (curVal >= SAMPLESPERPOINT)
    {
        //playPauseResult(false);
        curVal = 0;
    }

    ui->horizontalSliderFrame->setValue(curVal);
    /*
    {
        static QElapsedTimer t;
        qDebug()<<"incrSlider: curVal" << curVal <<"sliderIncVal"<<sliderIncVal: <<"Inter increment time: "<<t.elapsed();
        t.start();
    }
    */
}

void MainWindow::on_enumPlaySpeed_currentIndexChanged(int index)
{
    if(ui->pushButtonPlayPause->isChecked()== TRUE)
    {

        playPauseResult(false);
        playPauseResult(true);
/*
        mainTimer->stop();

        mainTimer->start(100/(1 << index ));
*/
    }
}

void MainWindow::on_groupBoxFilterStep1_toggled(bool arg1)
{
    if (arg1==false)
        ui->groupBoxFilterStep1->setChecked(false);//must always have a step 1`
}

void MainWindow::on_groupBoxFilterStep2_toggled(bool arg1)
{
    if (arg1==true)
        ui->groupBoxFilterStep1->setChecked(true);//must always have a step 1`
}

void MainWindow::on_pushButtonProcessFilter_clicked()
{
    //ui->pushButton->setEnabled(false);
    this->updateResultParStruct();
    emit postProcessingFilteringRequired();
}


void MainWindow::on_pushButtonCapture_clicked()
{
    connect(ui->dial_intensity,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
    disconnect(ui->dial_intensity_2,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
    ui->dial_intensity_2->hide();
    dataProc->saveScreenshot();
    emit ui->dial_intensity->valueChanged(ui->dial_intensity->value()+1);
    emit ui->dial_intensity->valueChanged(ui->dial_intensity->value()-1);
}

void MainWindow::on_pushButtonProcessVtwam_clicked()
{
    QString VTWAMtitle;
    QString tempStr;
    int index = 0;

    if(resultInfo.vtwamRangeNo == -1)
    {
        //on_pushButtonVtwamAddRange_clicked();
        return;
    }

    VTWAMtitle = "VTWAM Result(";
    do
    {
        if (index>0)
            VTWAMtitle +=",";

        double startTime = (double)resultInfo.vtwamStartFr[index]/(double)daqInfo.SamplingFreq;
        double endTime =   (double)resultInfo.vtwamEndFr[index]/(double)daqInfo.SamplingFreq;

        tempStr.sprintf("%.3f \xC2\xB5s ~ %.3f \xC2\xB5s",startTime,endTime);

        VTWAMtitle += tempStr;
        index++;

    }while(index<=resultInfo.vtwamRangeNo);

    VTWAMtitle +=")";

   //this->updateResultParStruct();
   //show the second knob and allocate proper scale
   disconnect(ui->dial_intensity,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
   connect(ui->dial_intensity_2,SIGNAL(valueChanged(int)),qwtSpectrogram2,SLOT(setIntensity(int)));
   ui->dial_intensity_2->show();
   emit ui->dial_intensity_2->valueChanged(ui->dial_intensity_2->value()+1);
   emit ui->dial_intensity_2->valueChanged(ui->dial_intensity_2->value()-1);
   emit postProcessingVtwamRequired(VTWAMtitle);
}

void MainWindow::on_pushButtonVtwamAddRange_clicked()
{
    resultInfo.vtwamRangeNo = (resultInfo.vtwamRangeNo + 1) % MAXVTWAMRANGES;
    this->updateResultParStruct();
    printVtwamInfo();
}

void MainWindow::on_pushButtonVtwamClearRanges_clicked()
{
    resultInfo.vtwamRangeNo = -1;
    ui->labelVtwamInfo->setText("");
    ui->labelVtwamRangeTitle->hide();
}

void MainWindow::printVtwamInfo()
{
    QString label,tempStr;
    double startTime = (double)resultInfo.vtwamStartFr[resultInfo.vtwamRangeNo]/(double)daqInfo.SamplingFreq;
    double endTime = (double)resultInfo.vtwamEndFr[resultInfo.vtwamRangeNo]/(double)daqInfo.SamplingFreq;

    tempStr.sprintf("%.3f \xC2\xB5s ~ %.3f \xC2\xB5s",startTime,endTime);

    if (resultInfo.vtwamRangeNo>0)
        label = ui->labelVtwamInfo->text() + ", " + tempStr + ",";

    else
        label = tempStr;

    ui->labelVtwamInfo->setText(label);
    ui->labelVtwamRangeTitle->show();

    //qwtPlotResult->updateAxisScale();
}

void MainWindow::updateVtwamInputs(bool press, int frValue)
{
    QString timeVal;// = QString::number(((double)frValue/daqInfo.SamplingFreq) ,'g',3);

    timeVal.sprintf("%.3f",((double)frValue/daqInfo.SamplingFreq));

    if (press)
    {
        //ui->lineEditVtwamStart->setText(QString::number(value));
        ui->lineEditVtwamStart->setText(timeVal);
    }
    else
    {
        //ui->lineEditVtwamEnd->setText(QString::number(value));
        ui->lineEditVtwamEnd->setText(timeVal);
    }
}

void MainWindow::on_checkBoxTT_toggled(bool checked)
{
    UpdateScanParStruct();
}

void MainWindow::on_pushButtonEnlarge_clicked()
{
    enlargeResultDlgn->attachSpect(qwtSpectrogram);

    ui->groupBoxResultControl->setParent(enlargeResultDlgn);
    ui->groupBoxResultControl->move(0,0);
    ui->groupBoxResultControl->showMaximized();

    qwtSpectrogram->enlargeEnabled = TRUE;
    qwtSpectrogram->updateAxisXY(enlargeResultDlgn->giveSlider()->value());
}

void MainWindow::resizeToNormal()
{
    qwtSpectrogram->setParent(ui->widgettResultSpect);
    qwtSpectrogram->enlargeEnabled = FALSE;
    qwtSpectrogram->updateAxisXY();
    qwtSpectrogram->show();

    ui->groupBoxResultControl->setParent(this->ui->widgetResultControl);
    ui->groupBoxResultControl->move(-5,10);
    ui->groupBoxResultControl->show();
}

//-------------------------------------------------Subband tab----------------------------------------
void MainWindow::playPauseResultSubband(bool play)
{
    if (play)
    {
       incrSliderSubband();
       mainTimerSubband->start(50 /(1 << ui->enumPlaySpeed->currentIndex()) );
       ui->pushButtonPlayPauseSubband->setText("Pause");
       ui->pushButtonPlayPauseSubband->setStatusTip("Pause the automatic display of data.");
       ui->pushButtonPlayPauseSubband->setChecked(true);
    }
    else
    {
        mainTimerSubband->stop();
        ui->pushButtonPlayPauseSubband->setText("Play");
        ui->pushButtonPlayPauseSubband->setChecked(false);
        ui->pushButtonPlayPauseSubband->setStatusTip("Automatically traverse the data in a frame-wise fashion. ");
    }
}

void MainWindow::incrSliderSubband()
{
    int curVal = ui->horizontalSliderSubband->value();
    ui->horizontalSliderSubband->setValue(++curVal);
    if (curVal == SAMPLESPERPOINT)
        playPauseResultSubband(false);
}

void MainWindow::on_enumPlaySpeedSubband_currentIndexChanged(int index)
{
    if(ui->pushButtonPlayPauseSubband->isChecked()== TRUE)
    {
        mainTimerSubband->stop();
        mainTimerSubband->start(50/(1 << index ));
    }
}

void MainWindow::on_radioButtonSubband1_toggled(bool checked)
{
    dataProc->chooseSubband(0);
}

void MainWindow::on_radioButtonSubband2_toggled(bool checked)
{
    dataProc->chooseSubband(1);
}

void MainWindow::on_radioButtonSubband3_toggled(bool checked)
{
    dataProc->chooseSubband(2);
}
//-------------------------------------------------------------------------------- slider
MySlider::MySlider(QWidget *parent):
QSlider(parent)
{
/* // line beneath the scaleDraw
    line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Raised);
    line->setVisible(false); //not working
*/
}

void MySlider::mousePressEvent ( QMouseEvent * event )
{
    if (event->button() == Qt::MiddleButton)
    {
        qDebug()<<"middle button pressed"<<event->pos().x()<<event->pos().y();
        xAtpress = event->pos().x();

        emit mouseMidButton(1,this->value());
    }
  QSlider::mousePressEvent(event);

}
void MySlider::mouseReleaseEvent ( QMouseEvent * event )
{
    if (event->button() == Qt::MiddleButton)
    {
        qDebug()<<"middle button released"<<event->pos().x()<<event->pos().y();
/*
        //line->setGeometry(QRect(xAtpress, (this->pos().y()), event->pos().x(), (this->pos().y())+5));
        this->line->setGeometry(QRect(xAtpress, 13, event->pos().x(), 13));
        //line->setGeometry(this->rect());
        this->line->setFrameShape(QFrame::HLine);
        this->line->setFrameShadow(QFrame::Sunken);
*/

        emit mouseMidButton(0,this->value());
    }
  QSlider::mouseReleaseEvent(event);
}

void MainWindow::on_lineEditStandofDistance_editingFinished()
{
    UpdateLmsParStruct();
}

void MainWindow::on_spinBoxLaserStep_editingFinished()
{
    UpdateLmsParStruct();
}

void MainWindow::on_checkBoxDisplayScanArea_clicked(bool checked)
{
    UpdateLmsParStruct();
    lmsInfo.diplayArea=checked;
}

void MainWindow::on_pushButtonConfigOsci_clicked()
{
    UpdateDaqParStruct();
    daq->Configure(true);
}

void MainWindow::on_checkBoxEnableMultiBand_toggled(bool checked)
{
    ui->enumMultiBandSettingLev1->clear();
    ui->checkBox_useCurrentResults->setChecked(false);
    ui->enumTotalScans->setCurrentIndex(0);
    if (checked == true)
    {
        ui->enumMultiBandSettingLev1->addItem("Broadband");
        ui->enumMultiBandSettingLev1->addItem("Narrowband");
        ui->enumMultiBandSettingLev1->addItem("Highfreq");

//        windowLayOutUpdate(true);#subband
    }
    else
    {
        ui->enumMultiBandSettingLev1->addItem("050-250 kHz");
        ui->enumMultiBandSettingLev1->addItem("250-500 kHz");
        ui->enumMultiBandSettingLev1->addItem("500-750 kHz");
        ui->enumMultiBandSettingLev1->addItem("750-1000 kHz");
        ui->enumMultiBandSettingLev1->addItem("1000-1160 kHz");
        ui->enumMultiBandSettingLev1->addItem("1160-1320 kHz");
        ui->enumMultiBandSettingLev1->addItem("1320-1500 kHz");

//        windowLayOutUpdate(false);#subband
    }
}

void MainWindow::on_pushButtonFilterConfig_pressed()
{
    //ascertain the setting number to use
    if (ui->checkBoxEnableMultiBand->isChecked() == true)
    {
        //the broadBand
        settingNumber = ui->enumMultiBandSettingLev1->currentIndex();

        if (ui->enumMultiBandSettingLev1->currentIndex() == 1)//narrow
            settingNumber = ui->enumMultiBandSettingLev2->currentIndex()+1;

        else if (ui->enumMultiBandSettingLev1->currentIndex() == 2)//hi
            settingNumber = 4;
    }
    else
    {
        settingNumber = ui->enumMultiBandSettingLev1->currentIndex()+5;
    }

    UpdateDaqParStruct();

//    windowLayOutUpdate(settingArr[settingNumber].chNum > 1); #subband

    /*
    daqInfo.settingStr = "Number of Bands = "+ QString::number(settingArr[settingNumber].chNum)+
            ", Sample Freq = "+ QString::number(settingArr[settingNumber].samplingFreq)+" MHz"
            ", LDV Range = "+ QString::number(settingArr[settingNumber].ldvRange)+" mm/s/V";
    */
    daqInfo.settingStr.clear();

//    for (int chNumber = 1; chNumber<=settingArr[settingNumber].chNum; chNumber++ )#multichannel
    int chNumber = 1;

        filter->config(chNumber,settingArr[settingNumber].filtPar[chNumber].gain,
                       settingArr[settingNumber].filtPar[chNumber].hiPassCut,
                       settingArr[settingNumber].filtPar[chNumber].lowPassCut);

        daqInfo.settingStr += "Sub-band " + QString::number(chNumber) +
                " : "+ QString::number(settingArr[settingNumber].filtPar[chNumber].hiPassCut)+" KHz"
                " ~ "+ QString::number(settingArr[settingNumber].filtPar[chNumber].lowPassCut)+" KHz\n";


    setLdvRange();//ldv->setRange();
    daq->Configure(true);

    qDebug()<<daqInfo.settingStr;
    this->ui->labelConfigSetting->setText(daqInfo.settingStr);
    qwtPlotOsc->updateAxisScale();
    qwtPlotResult->updateAxisScale();
    qwtSpectrogram->updateAxisXY();
//    this->ui->labelConfigSettingSubBand->setText(daqInfo.settingStr);
}

void MainWindow::on_enumMultiBandSettingLev1_currentIndexChanged(int index)
{
    if (index == 1 && ui->checkBoxEnableMultiBand->isChecked() == true)
       ui->enumMultiBandSettingLev2->show();
    else
       ui->enumMultiBandSettingLev2->hide();
}
