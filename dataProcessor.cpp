#include "dataProcessor.h"
#include <QtWinExtras\QtWin>

dataProcessor::dataProcessor(structDaq *daqInfoPtrArg, structScan *scanInfoPtrArg, structResult *resultInfoArg,
                             spectrogram* qwtSpectrogramArg, spectrogram *qwtSpectrogram2Arg, QString progDataPathArg,
                             spectrogram** qwtSpectrogramSubbandArrPtrArg,QLabel * labelMovieDebugArg)
{
    daqInfoPtr              = daqInfoPtrArg;
    scanInfoPtr             = scanInfoPtrArg;
    resultInfoPtr           = resultInfoArg;
    dataBundle              = NULL;
    resultTimeSig           = NULL;
    vtwamResult             = NULL;
    screenShotResult        = NULL;
    buffScanPoints          = 0;
    frameNum                = 0;
    frameNumSubband         = 0;
    dispBuffIndex           = 0;
    qwtSpectrogram          = qwtSpectrogramArg;
    qwtSpectrogram2         = qwtSpectrogram2Arg;
    dataBundleSaved[0]      = false;
    dataBundleSaved[1]      = false;
    progDataPath            = progDataPathArg;
    VtwamProcessing         = false;
    bufToProcess            = 1;
    qwtSpectrogramSubbandArrPtr = qwtSpectrogramSubbandArrPtrArg;
    chosenSubband           = 0; // incase of no subband deComposition
    subbandDecomp           = false;
    labelMovieDebug         = labelMovieDebugArg;
    rptScanAccDataBundle    = NULL;
    applyRealTimeFilter     = false;
    boostSpeedEn            = true;
    postProcDone            = false;
    VTWAMTitle              = "VTWAM Result";

    resultTimeSig = (short *)calloc(SAMPLESPERPOINT,sizeof (short)) ;
    //initFilterTabs();

}

dataProcessor::~dataProcessor()
{
    if (clear2dArray16bit(SAMPLESPERPOINT,buffScanPoints))
    {
         qDebug("Error: dataProcessor::~dataProcessor() - dataBundle cleared - buffScanPoints : %d", buffScanPoints);
    }
    else
    {
         qDebug("Error: dataProcessor::~dataProcessor() - cant clear dataBundle - buffScanPoints : %d", buffScanPoints);
    }

    if (resultTimeSig!=NULL)
    {
        free(resultTimeSig);
        resultTimeSig = NULL;
    }

    //delete this->filterTabDataBase;
}

#define NUMOFFILTERS    2
void dataProcessor::initFilterTabs()
{
    this->filterTabDataBase = (structFilterTabs *)new structFilterTabs[NUMOFFILTERS];

    //50~200KHz @10MHz
    filterTabDataBase[0].SamplingFreqMhz        =   10;
    filterTabDataBase[0].startFreqBandPassKhz   =   50;
    filterTabDataBase[0].stopFreqBandPassKhz    =   200;
    filterTabDataBase[0].a2 = -3.85925619825728;
    filterTabDataBase[0].a3 =  5.59423437338994;
    filterTabDataBase[0].a4 = -3.61017813077089;
    filterTabDataBase[0].a5 =  0.875214548253683;
    filterTabDataBase[0].b1 =  0.00208056713552316;
    filterTabDataBase[0].b2 =  0;
    filterTabDataBase[0].b3 = -0.00416113427104632;
    filterTabDataBase[0].b4 =  0;
    filterTabDataBase[0].b5 =  0.00208056713552316;

    //200~500KHz @10MHz
    filterTabDataBase[1].SamplingFreqMhz        =   10;
    filterTabDataBase[1].startFreqBandPassKhz   =   200;
    filterTabDataBase[1].stopFreqBandPassKhz    =   500;
    filterTabDataBase[1].a2 = -3.66102925645485;
    filterTabDataBase[1].a3 =  5.09866291711957;
    filterTabDataBase[1].a4 = -3.20227713766829;
    filterTabDataBase[1].a5 =  0.766006600943265;
    filterTabDataBase[1].b1 =  0.00782020803350215;
    filterTabDataBase[1].b2 =  0;
    filterTabDataBase[1].b3 = -0.0156404160670043;
    filterTabDataBase[1].b4 =  0;
    filterTabDataBase[1].b5 =  0.00782020803350215;
}

//checks the start/stop and sampling freq in the stored coeff database and returns the index of filter.
//returns -1 if the index is not found
int dataProcessor::getFiltCoeffIndex()
{
    int i;

    if (this->filterTabDataBase == NULL)
        return -1;

    for (i = 0;i<NUMOFFILTERS;i++)
    {
        if (filterTabDataBase[i].SamplingFreqMhz        == daqInfoPtr->SamplingFreq         &&
            filterTabDataBase[i].startFreqBandPassKhz   == daqInfoPtr->startFreqBandPass    &&
            filterTabDataBase[i].stopFreqBandPassKhz    == daqInfoPtr->stopFreqBandPass)
        {
            return i;
        }
    }
    return -1;
}

void dataProcessor::saveScreenshot()
{
    if (isMemValid(dataBundle, dispBuffIndex, frameNum,__FUNCTION__) == false)
        return;

    //save the actual plot
    QDateTime now = QDateTime::currentDateTime();
    QString captureStoragePath = progDataPath + "\\ScreenShots\\";
    QString captureFileName = "ScreenshotUWPI"+now.toString("ddMMyy_hhmmss");
    if( QDir(captureStoragePath).exists() == false)
        QDir().mkpath(captureStoragePath);
    qwtSpectrogram->savePlot(captureStoragePath+captureFileName);

    if (screenShotResult == NULL)
    {
          qWarning("dataProcessor::saveScreenshot: couldnt allocate screenShotResult");
          return;
    }

     timer.start();
     for(unsigned int i =0;i<this->buffScanPoints;i++)
         screenShotResult[i] = dataBundle[dispBuffIndex][frameNum][i];
     qDebug()<<timer.elapsed();


    qwtSpectrogram2->updateAxisXY();
    //qwtSpectrogram2->updateData((dataBundle[dispBuffIndex][frameNum]),captureFileName);
    qwtSpectrogram2->updateData(screenShotResult,captureFileName);

    //to display a frame give to dataProc and then qwtSpectrogram
}

void dataProcessor::saveScreenshotVtwam()
{
    //show the result if not already shown
    qwtSpectrogram2->updateData(this->vtwamResult,this->VTWAMTitle, 1);
    qwtSpectrogram2->updateAxisXY();

    //save the actual plot
    QDateTime now = QDateTime::currentDateTime();
    QString captureStoragePath = progDataPath + "\\ScreenShots";
    QString captureFileName = "\\ScreenshotVTWAM"+now.toString("ddMMyy_hhmmss");
    if( QDir(captureStoragePath).exists() == false)
        QDir().mkpath(captureStoragePath);
    qwtSpectrogram2->savePlot(captureStoragePath+captureFileName);
}

void dataProcessor::saveMovie()
{
    timer.start();

    if (isMemValid(dataBundle, dispBuffIndex, frameNum,__FUNCTION__) == false)
        return;

    qwtSpectrogram->setMovieImageSize();

    //make storage path
    QDateTime now = QDateTime::currentDateTime();
    QString captureStoragePath = progDataPath + "\\Movies";
    QString captureFileName = "\\Movie"+now.toString("ddMMyy_hhmmss")+".avi";
    if( QDir(captureStoragePath).exists() == false)
        QDir().mkpath(captureStoragePath);

    QString StatusTip;
    QByteArray fileArr = QString(captureStoragePath+captureFileName).toUtf8();
    HBITMAP hbm;
    //
    HAVI avi = CreateAvi(fileArr.toStdString().c_str(),100,NULL);
    //QList<QPixmap> images;
    QPixmap pixMapImage;
    int savedFrameNum = this->frameNum;

    //for (int frame=0; frame < 20; frame++)
    for (int frame=0; frame < SAMPLESPERPOINT; frame++)
    {
        setframeNum(frame);
        pixMapImage = qwtSpectrogram->getPlotPixmap(frame);
        {
            //QLabel imageLabel(this->qwtSpectrogram2);
            //imageLabel= new QLabel(this);
        //    labelMovieDebug->setPixmap(pixMapImage);
        //    labelMovieDebug->showFullScreen();
            //setCentralWidget(imageLabel);
        }

        hbm = QtWin::toHBITMAP(pixMapImage); // memory leaking culprit
        AddAviFrame(avi,hbm);
        DeleteObject(hbm);
        QCoreApplication::processEvents();
        emit updateProgressBar_dataProcessorSignal( ( (double)(frame+1)/SAMPLESPERPOINT)*100 );
        StatusTip = "Generating movie - "+QString::number(frame+1)+"/"+QString::number(SAMPLESPERPOINT)+"frames";
        emit updateStatusBar_dataProcessorSignal(StatusTip);
    }
    CloseAvi(avi);
    qDebug()<<"Movie done in "<<timer.elapsed();

    setframeNum(savedFrameNum);
}

void dataProcessor::plotResultTime(int pointToPlot)
{
    int x_length = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;

    float x = (scanInfoPtr->scanInterval*(pointToPlot%x_length) );
    float y = (scanInfoPtr->scanInterval*(pointToPlot/x_length));

//    float y = (scanInfoPtr->scanInterval*(pointToPlot%y_length) );
//    float x = (scanInfoPtr->scanInterval*(pointToPlot/y_length));


    if (resultTimeSig!=NULL)
    {
        if (isMemValid(dataBundle, dispBuffIndex, frameNum,__FUNCTION__) == false)
            return;

        for (int i=0;i<SAMPLESPERPOINT;i++)
            resultTimeSig[i] = dataBundle[dispBuffIndex][i][pointToPlot];
    }

    QString title = "x=" +  QString::number(x) + ",y=" + QString::number(y);
    emit updateResultTimePlot(resultTimeSig,title);
#if ACTUALSYSTEM
    emit setStagePosX(x*1000,true); // set stage pos x relative to scan start coordinates
    emit setStagePosZ(y*1000,true); // set stage pos z relative to scan start coordinates
#endif

}

bool dataProcessor::allocateMem()
{
    bool allocSuccess = true;
    //This can be called from the mainwindow or called when we recieve the 1st waveform from daq.
    MEMORYSTATUSEX memory_status;
    ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
    memory_status.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memory_status);
    qint64 sysPhyMem = memory_status.ullTotalPhys >> 20;

    if (daqInfoPtr->subbandDecomp==false)
    {
        //number of frames* (1d pointer size + actual data for each frame) conversion to MB
        qint64 MemoryNeeded = (SAMPLESPERPOINT*(sizeof(short*) + (qint64)daqInfoPtr->ScanPoints*sizeof(short)))>>20;
        qint64 MemoryNeededforDataBuffs = MemoryNeeded<<1; //2 x data buffer
        qint64 MemoryforSigLengthFifo = 256;
        qint64 totalMemReq = MemoryNeededforDataBuffs + MemoryforSigLengthFifo;
        qint64 memForRptScanBuf;

        if (scanInfoPtr->scansPerInspection > 1 || scanInfoPtr->useCurrentResults)
            memForRptScanBuf = (SAMPLESPERPOINT*(sizeof(int*) + (qint64)daqInfoPtr->ScanPoints*sizeof(int)))>>20;
        else
            memForRptScanBuf = 0;

        totalMemReq += memForRptScanBuf;

        qDebug()<<"dataProcessor::allocateMem - daqInfoPtr->ScanPoints: "<< daqInfoPtr->ScanPoints
               <<"\nSize per buff(MB):" <<MemoryNeeded
               <<"\nTotal Size buffs(MB):"<<MemoryNeededforDataBuffs
               <<"\nSig lenght FIFO(MB):" << MemoryforSigLengthFifo
               <<"\nSize of rpt scan buff:"<<memForRptScanBuf
               <<"\nTotal Mem Requirement:" << totalMemReq
               <<"\nSystem physical memory(MB):"<< sysPhyMem;

        if ((totalMemReq + 3000) > sysPhyMem)
        {
            QMessageBox msgBox(QMessageBox::Critical, tr("Insufficient Memory"),tr("Required memory: ") + QString::number(totalMemReq) + tr("\n System Memory: ") + QString::number(sysPhyMem), 0);
            msgBox.exec();
            return 0;
        }
    }
    else
        if (daqInfoPtr->subbandDecomp){
            qint64 MemoryNeeded = (4000+SAMPLESPERPOINT*2*(qint64)daqInfoPtr->ScanPoints)>>20;
            qDebug()<<"dataProcessor::allocateMem - daqInfoPtr->ScanPoints: "<< daqInfoPtr->ScanPoints
                   <<"Size per buff(MB): " <<MemoryNeeded<<"Total Size (MB): "<<MemoryNeeded * (NUMOFBANDS); // row pt mem + data mem
        }

    if (dataBundle == NULL) // allocate if not done already.
    {
        //declare 2D array via pointer.
        if (allocSuccess = alloc2dArray16bit(SAMPLESPERPOINT,daqInfoPtr->ScanPoints))
        {
            buffScanPoints = daqInfoPtr->ScanPoints; // this will be used in clearing up the memory if the scan pars are changed next time.
            subbandDecomp = daqInfoPtr->subbandDecomp;
            buffScanWidth  = scanInfoPtr->scanWidth;
            buffScanHeight  = scanInfoPtr->scanHeight;
            buffScanInterval  = scanInfoPtr->scanInterval;
            rptScansPerInspection =  scanInfoPtr->scansPerInspection;
            useCurrentResults = scanInfoPtr->useCurrentResults;
        }
        else
        {
            //clear any allocated mem
            clear2dArray16bit(SAMPLESPERPOINT,buffScanPoints);
            qDebug("Error: dataProcessor::allocateMem() - cant declare dataBundle - daqInfoPtr->ScanPoints : %d", daqInfoPtr->ScanPoints);
        }
    }
    else if (scanInfoPtr->useCurrentResults)
    {
        //1.Do not clear the previous dataBundle(s)

        //2.Only create the rptScanAccumulatorBuffer

        //3.Add the current DataBundle[0] to this repeatScanBuffer.

        if (daqInfoPtr->ScanPoints      != buffScanPoints    ||
            scanInfoPtr->scanWidth      != buffScanWidth     ||
            scanInfoPtr->scanHeight     != buffScanHeight    ||
            scanInfoPtr->scanInterval   != buffScanInterval     )
        {
            QMessageBox msgBox(QMessageBox::Critical, tr("Repeat Scan Error"),tr("'Use Current result' is checked. Please do not change the scan area and scan interval for the new scan."),QMessageBox::NoButton, 0);
            msgBox.exec();
            return false;

        }

        if (allocSuccess = clear2dArray16bit(SAMPLESPERPOINT,buffScanPoints,true))
        {
            if (allocSuccess = alloc2dArray16bit(SAMPLESPERPOINT,daqInfoPtr->ScanPoints,true))
            {
                rptScansPerInspection   =  scanInfoPtr->scansPerInspection;
                useCurrentResults       = scanInfoPtr->useCurrentResults;
                addToRepeatScanBuffer();
            }
        }
    }
    //else if (daqInfoPtr->ScanPoints != buffScanPoints) // allocate if the size has changed
    else
    {
        //clear previous before allocating new.
        if (allocSuccess = clear2dArray16bit(SAMPLESPERPOINT,buffScanPoints))
        {
            if (allocSuccess = alloc2dArray16bit(SAMPLESPERPOINT,daqInfoPtr->ScanPoints))
            {
                buffScanPoints = daqInfoPtr->ScanPoints; // this will be used in clearing up the memory if the scan pars are changed next time.
                subbandDecomp = daqInfoPtr->subbandDecomp;
                buffScanWidth  = scanInfoPtr->scanWidth;
                buffScanHeight  = scanInfoPtr->scanHeight;
                buffScanInterval  = scanInfoPtr->scanInterval;
                rptScansPerInspection =  scanInfoPtr->scansPerInspection;
                useCurrentResults = scanInfoPtr->useCurrentResults;
            }
            else
                qDebug("Error: dataProcessor::allocateMem() - cant declare dataBundle - daqInfoPtr->ScanPoints : %d", daqInfoPtr->ScanPoints);
        }
        else
        {
             qDebug("Error: dataProcessor::allocateMem() - cant clear dataBundle - buffScanPoints : %d", buffScanPoints);
        }
    }

    if (allocSuccess)
    {
        VTWAMTitle          = "VTWAM Result";
        postProcDone        = false;
        dataBundleSaved[0]  = false;
        dataBundleSaved[1]  = false;

        //makesure the scan info has correct info.
        if (screenShotResult != NULL)
        {
            free(screenShotResult);
            screenShotResult=NULL;
        }

         screenShotResult = (short*) calloc(this->buffScanPoints,sizeof(short));
         qwtSpectrogram2->updateData(screenShotResult," ",0);
         //qwtSpectrogram2->updateAxisXY();

         //makesure the scan info has correct info.
         if (vtwamResult != NULL)
         {
             free(vtwamResult);
             vtwamResult=NULL;
         }

          vtwamResult = (unsigned int*) calloc(this->buffScanPoints,sizeof(unsigned int));

          this->setframeNum(frameNum);
          qwtSpectrogram->updateAxisXY();

          if (subbandDecomp)
          {
              for (int i=0;i<NUMOFBANDS;i++)
              {
                  qwtSpectrogramSubbandArrPtr[i]->updateAxisXY();
                  dataBundleSaved[i] = false;
              }
              setframeNumSubband(frameNumSubband);
          }

    }
    return allocSuccess;
}

//DF2 filter
inline short dataProcessor::IIRfilter(short in,bool reset,int filtCoeffIndex)
{
   static double inAcc,outAcc;
   static double buf1,buf2,buf3,buf4;

   if (reset == true) // reset the state vars when new filtering starts
       inAcc=outAcc=buf1=buf2=buf3=buf4=0.0;

    //200-500
    //double a1 = 1, a2 = -3.95340937166299, a3 = 5.86341125073283, a4 = -3.86654437944130, a5 = 0.956543676511204;
    //double b1 = 0.000241359048974918,	b2=0.0,	b3=-0.000482718097949836,	b4=0.0,	b5=0.000241359048974918;

    //10k~90k
    //double a1 = 1, a2 = -3.98813276868391, a3 =	5.96448787337403, a4 = 	-3.96457732426005, a5 =	0.988222219666761;
    //double b1 = 1.74425378099272e-05,b2 = 0.0, b3 = -3.48850756198545e-05, b4 = 0.0,b5 = 1.74425378099272e-05;


    //input acc for the IIR
    inAcc = in;
    inAcc = inAcc - (filterTabDataBase[filtCoeffIndex].a2*buf1);
    inAcc = inAcc - (filterTabDataBase[filtCoeffIndex].a3*buf2);
    inAcc = inAcc - (filterTabDataBase[filtCoeffIndex].a4*buf3);
    inAcc = inAcc - (filterTabDataBase[filtCoeffIndex].a5*buf4);

    //output acc for the IIR (FIR part in IIR)
    outAcc = filterTabDataBase[filtCoeffIndex].b1*inAcc;
    outAcc = outAcc + (filterTabDataBase[filtCoeffIndex].b2*buf1);
    outAcc = outAcc + (filterTabDataBase[filtCoeffIndex].b3*buf2);
    outAcc = outAcc + (filterTabDataBase[filtCoeffIndex].b4*buf3);
    outAcc = outAcc + (filterTabDataBase[filtCoeffIndex].b5*buf4);

    buf4 = buf3;
    buf3 = buf2;
    buf2 = buf1;
    buf1 = inAcc;

    return(round(outAcc));
}

void dataProcessor::setRlTmMdFlt(bool applyRealTimeFilterArg)
{
    this->applyRealTimeFilter = applyRealTimeFilterArg;
}

void dataProcessor::newWfmCopyToArray_slot(short *wfmPtr, int waveformNumber, short burstSize)
{
    bool updateResult=false;
    int x_length = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    int y_length = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;
    int filtCoeffIndex = -1;
    int waveformNumberlocal;
    bool isOddLine;
    bool isOddFrame;

    int shift = 0;
    static int wfmToFrameConvFactor_oddLine;

    int rowNumber=0;
    int shiftSideLimit = 0;
    short filteredPt;

    if (scanInfoPtr->PRF.toFloat()< 1)
    shift =3;
    else if (scanInfoPtr->PRF.toFloat()>= 1 && scanInfoPtr->PRF.toFloat()<= 3)
    shift =3;
    else if (scanInfoPtr->PRF.toFloat()> 3 && scanInfoPtr->PRF.toFloat()<= 7)
    shift =5;
    else if (scanInfoPtr->PRF.toFloat()> 7 && scanInfoPtr->PRF.toFloat()<= 15)
    shift =7;
    else if (scanInfoPtr->PRF.toFloat()> 2 && scanInfoPtr->PRF.toFloat()<= 20)
    shift =9;

    if (waveformNumber==0) //memory sanity check once at start
    {
        if (isMemValid(dataBundle, 0, frameNum,__FUNCTION__,false) == false)
            return;
        if (wfmPtr == NULL)
        {
            QMessageBox msgBox(QMessageBox::Critical, tr("Memory Error"),tr("dataProcessor::scanFinished - wfmPtr = NULL"), 0);
            msgBox.exec();
            return;
        }

        if (applyRealTimeFilter)
            startParallelProcThread(1,1,dataBundle[1],dataBundle[0],x_length,y_length); // start the real time median mask
    }

    for (unsigned int framePoint=waveformNumber;framePoint<waveformNumber+burstSize;framePoint++)
    {
        if (waveformNumberlocal<buffScanPoints)
        {
        waveformNumberlocal = framePoint;
        rowNumber = waveformNumberlocal/x_length;
        shiftSideLimit = waveformNumberlocal-(x_length*rowNumber);

        isOddLine =(waveformNumberlocal/x_length) % 2;

            if (isOddLine)
                framePoint += wfmToFrameConvFactor_oddLine;

            if (framePoint <0)
                framePoint=0;
            else if (framePoint >(x_length*y_length)-1)
                framePoint=(x_length*y_length)-1;


        for (unsigned int frame=0;frame<SAMPLESPERPOINT;frame++)
        {
            int pt= frame+((waveformNumberlocal-waveformNumber)*SAMPLESPERPOINT);
            filteredPt =wfmPtr[pt];// framePoint;//colNumber;//wfmPtr[pt];

                  if (shiftSideLimit > shift)
                  {
                        if (applyRealTimeFilter)
                            dataBundle[1][frame][framePoint]    += filteredPt;
                        else
                            dataBundle[0][frame][framePoint]    += filteredPt;
                  }
                  else
                  {

                    if (applyRealTimeFilter)
                        dataBundle[1][frame][framePoint] = 0;
                    else
                        dataBundle[0][frame][framePoint] = 0;
                  }

        }
        if ( isOddLine) //for odd rows
            wfmToFrameConvFactor_oddLine-=2;
        else //for even rows
            wfmToFrameConvFactor_oddLine = x_length-1+shift;

        if(  (((waveformNumberlocal+1)%((x_length))) == 0) )
        {
            emit updateRowColNumAcquired_sig(waveformNumberlocal);

            #ifdef DAQ_DEBUG_LOGS
            //qDebug()<<QDateTime::currentDateTime().time()<<"newWfmCopyToArray_slot - waveformNumber"<<waveformNumber<<
              //        "isOddLine"<<isOddLine;
            #endif
        }

        else
        {
            #ifdef DAQ_DEBUG_LOGS
            //qDebug()<<QDateTime::currentDateTime().time()<<"newWfmCopyToArray_slot - waveformNumber"<<waveformNumber<<"isOddLine"<<isOddLine<<
              //    "wfmPtr"<<wfmPtr;
            #endif
        }
        framePoint = waveformNumberlocal;
    }
    }

    setframeNum(frameNum);
    qDebug()<<QDateTime::currentDateTime().time()<<"buffScanPoints"<<buffScanPoints<<"waveformNumber"<<waveformNumber <<"waveformNumberlocal"<<waveformNumberlocal;

    emit wfmCopyDone_sig(waveformNumber);
}

void dataProcessor::rptScanFinished()
{
    //copy the rptScanAccumulated data back to buffer0 and kick off the default filtering on it.
    int tempForSat;
    QString StatusTip;

    int x_length = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    int y_length = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;

    //copy
    timer.start();
    for (unsigned int frame=0;frame<SAMPLESPERPOINT;frame++)
    {
        for (unsigned int framePoint=0; framePoint<this->buffScanPoints; framePoint++)
        {
            tempForSat = rptScanAccDataBundle[frame][framePoint]/(rptScansPerInspection+useCurrentResults);

            if (tempForSat>32767)
                tempForSat = 32767;
            if (tempForSat<-32768)
                tempForSat = -32768;

            dataBundle[0][frame][framePoint] = (short)tempForSat;
        }

        StatusTip.clear();
        emit updateProgressBar_dataProcessorSignal( ( ((double)frame+1)/SAMPLESPERPOINT)*100 );
        QTextStream(&StatusTip)<<" Averaged multiple scan data "<<frame+1<<"/"<<SAMPLESPERPOINT<<" frames";
        emit updateStatusBar_dataProcessorSignal(StatusTip);

        QCoreApplication::processEvents();
    }
    qDebug()<<"dataProcessor::rptScanFinished - Finished averaging databundle[0] in: "<<timer.elapsed()<<"(msec)";

    setframeNum(frameNum);

    /*
    //apply default filter
    chosenSubband = 0;
    dispBuffIndex = 0;
    this->filterItr    = 0; // this will be increased once an iteration is finished
    this->defaultFilter= true;
    this->bufToProcess = 0;
    startProcThread(1,1,dataBundle[0],dataBundle[0],x_length,y_length); // start the post processing on band1
    */
}

void dataProcessor::scanFinished(int waveformNumber)
{
    qDebug()<<"dataProcessor::scanFinished - waveformNumber:"<<waveformNumber;
   // if (waveformNumber<daqInfoPtr->ScanPoints)
   // {
        //emit updateOsciPlot((double*) &wfmPtr[waveformNumber*SAMPLESPERPOINT]);
   // }

    //make and fire the 3X3 median thread
    int x_length       = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    int y_length       = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;

    if (this->subbandDecomp==true)
    {
         this->filterItr    = 0; // this will be increased once an iteration is finished
         this->defaultFilter= true;
         this->bufToProcess = 1;
         //startProcThread(1,1,dataBundle[1],dataBundle[1],x_length,y_length); // start the post processing on band1
    }
    else
    {
         chosenSubband = 0;
         dispBuffIndex = 0;
         this->filterItr    = 0; // this will be increased once an iteration is finished
         this->defaultFilter= true;
         this->bufToProcess = 0;

         if (rptScansPerInspection > 1 || useCurrentResults)
             addToRepeatScanBuffer();

         //startProcThread(1,1,dataBundle[0],dataBundle[0],x_length,y_length); // start the post processing on band1
    }

    emit abortThread_sig();
}


void dataProcessor::addToRepeatScanBuffer()
{
    QString StatusTip;
    //copy
    timer.start();
    for (unsigned int frame=0;frame<SAMPLESPERPOINT;frame++)
    {
        for (unsigned int framePoint=0; framePoint<this->buffScanPoints; framePoint++)
        {
            rptScanAccDataBundle[frame][framePoint] += dataBundle[0][frame][framePoint];
        }

        StatusTip.clear();
        emit updateProgressBar_dataProcessorSignal( ( ((double)frame+1)/SAMPLESPERPOINT)*100 );
        QTextStream(&StatusTip)<<" Accumulating repeat scan data"<<frame+1<<"/"<<SAMPLESPERPOINT<<" frames";
        emit updateStatusBar_dataProcessorSignal(StatusTip);

        QCoreApplication::processEvents();
    }

    qDebug()<<"dataProcessor::addToRepeatScanBuffer - Finished accumulating databundle[0] in: "<<timer.elapsed()<<"(msec)";
}
void dataProcessor::chooseSubband(short chosenSubbandArg)
{
    if (subbandDecomp)
    {
        chosenSubband = chosenSubbandArg;
        dispBuffIndex = chosenSubband;
        setframeNum(frameNum);
    }
}

void dataProcessor::setframeNumSubband(int frameNum_arg)
{
    frameNumSubband = frameNum_arg;

    if (daqInfoPtr->subbandDecomp)
    {
        for (int i = 0;i<NUMOFBANDS;i++)
        {
            if (isMemValid(dataBundle, i, frameNumSubband,__FUNCTION__,true) == false)
                continue;

            qwtSpectrogramSubbandArrPtr[i]->updateData((short *)dataBundle[i][frameNumSubband]);
        }
    }
}

void dataProcessor::setframeNum(int frameNum_arg)
{
    frameNum = frameNum_arg;

    if (isMemValid(dataBundle, dispBuffIndex, frameNum,__FUNCTION__,true) == false)
        return;

    //makesure the scan info has correct info.
    buffScanWidth  = scanInfoPtr->scanWidth;
    buffScanHeight  = scanInfoPtr->scanHeight;
    buffScanInterval  = scanInfoPtr->scanInterval;

    QString labelString;
    if (daqInfoPtr->SamplingFreq != 0)
        labelString.sprintf("UWPI %.3f \xC2\xB5s",(double)frameNum_arg/daqInfoPtr->SamplingFreq);

    qwtSpectrogram->updateData((short *)dataBundle[dispBuffIndex][frameNum],labelString);
}

//simply selects which data is to be displayed, processed or un-processed.

void dataProcessor::selectDisplayBuffer(bool Filtered)
{
    if (daqInfoPtr->subbandDecomp)
    {
        if (Filtered == false)
            dispBuffIndex = chosenSubband;
        else
            dispBuffIndex = (chosenSubband+1)%(NUMOFBANDS);
    }
    else
    {
        if (Filtered == false)
        {
            dispBuffIndex = 0;
        }
        else if (Filtered == true && postProcDone == true)
        {
            dispBuffIndex = 1;
        }
    }
    setframeNum(frameNum);
}

void dataProcessor::postProcessingFilteringRequested()
{
    int x_length       = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    int y_length       = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;
    //inspect the result structure

    if (isMemValid(dataBundle, chosenSubband, frameNum,__FUNCTION__,false) == false)
        return;
      ////////// always apply filter to the data present in buffer 1 and save it to buffer 0;
    if ((resultInfoPtr->filtPass1en)   &&
        resultInfoPtr->filterType       != this->filterType   ||
        resultInfoPtr->filterRadius     != this->filterRadius ||
        resultInfoPtr->filterItr        != this->filterItr    ||

        ((resultInfoPtr->filtPass2en) &&
        resultInfoPtr->filterType2       != this->filterType2   ||
        resultInfoPtr->filterRadius2     != this->filterRadius2 ||
        resultInfoPtr->filterItr2        != this->filterItr2))
    {
        this->defaultFilter= false;
        this->filterItr = 0;
        this->filterItr2 = 0; //This will be triggered from within the scanfinished

        if (daqInfoPtr->subbandDecomp)
        {
            startProcThread(resultInfoPtr->filterType,resultInfoPtr->filterRadius,
                        dataBundle[chosenSubband],dataBundle[(chosenSubband+1)%(NUMOFBANDS)],x_length,y_length);
        }
        else
        {
            startProcThread(resultInfoPtr->filterType,resultInfoPtr->filterRadius,dataBundle[0],dataBundle[1],x_length,y_length);
        }
    }

    //qDebug()<<" dataProcessor::postProcessingFilteringRequested() - chosenSubband"<<chosenSubband<<"(chosenSubband+1)%(NUMOFBANDS)"<<(chosenSubband+1)%(NUMOFBANDS);
}

void dataProcessor::postProcessingVtwamRequested(QString VtwamTitleArg)
{
    if (isMemValid(dataBundle, dispBuffIndex, frameNum,__FUNCTION__) == false)
        return;

     if (vtwamResult == NULL)
     {
           qWarning("dataProcessor::postProcessingVtwamRequested: couldnt allocate vtwamResult");
           return;
     }
     this->VTWAMTitle = VtwamTitleArg;

     startVtwamThread(dataBundle[dispBuffIndex], vtwamResult, this->buffScanPoints);
}

void dataProcessor::updateResultSpect()
{
    if (daqInfoPtr->subbandDecomp)
    {
        setframeNumSubband(frameNum);
    }
    else
    {
       setframeNum(frameNum);
    }
}

void dataProcessor::parallelprocFinished()
{
    qDebug()<<"dataProcessor::parallelprocFinished().";
}

void dataProcessor::stop()
{
    emit abortThread_sig();
}

void dataProcessor::startParallelProcThread(short filtType ,short krnlRadius, short **Input,short **Output,int x_length,int y_length)
{
  this->coreProcObj = new coreProcessor(filtType,krnlRadius, Input,Output,x_length,y_length);
  this->procThread = new QThread;
  this->coreProcObj->moveToThread(procThread);

  //connections this<->coreProcObj
  //connect(fetcherObj, SIGNAL(errorSignal(QString)), this, SLOT(errorHandler(QString)));
  connect(coreProcObj, SIGNAL(updateProgress(int,short,short)), this, SLOT(updateResultSpect()));
  connect(this, SIGNAL(updateRowColNumAcquired_sig(int)), coreProcObj, SLOT(updateRowColNumAcquired(int)));
  connect(coreProcObj, SIGNAL(finished()), this, SLOT(parallelprocFinished()));

  //for aborting prematurely

  connect(this,SIGNAL(abortThread_sig()),coreProcObj,SLOT(setAbortThread()));

  //start process
  connect(procThread, SIGNAL(started()), coreProcObj, SLOT(processParallel()));
  //quit thread
  connect(coreProcObj, SIGNAL(finished()), procThread, SLOT(quit()));
  //delete after finishing
  connect(coreProcObj, SIGNAL(finished()), coreProcObj, SLOT(deleteLater()));
  connect(procThread, SIGNAL(finished()), procThread, SLOT(deleteLater()));

  this->procThread->start();
}

void dataProcessor::startVtwamThread(short **Input, unsigned int* result, unsigned int buffScanPoints)
{
    this->totalThreads = 1;
    this->coreProcObj = new coreProcessor(1,1,Input,NULL,50, 50, 0, 0, buffScanPoints,result,this->resultInfoPtr);
    this->procThread = new QThread;
    this->coreProcObj->moveToThread(procThread);

    //connect(fetcherObj, SIGNAL(errorSignal(QString)), this, SLOT(errorHandler(QString)));
    connect(coreProcObj, SIGNAL(updateProgress(int,short,short)), this, SLOT(updateProcProgress(int,short,short)));
    connect(coreProcObj, SIGNAL(finished()), this, SLOT(procFinished()));
    //start process
    connect(procThread, SIGNAL(started()), coreProcObj, SLOT(processVtwam()));
    //quit thread
    connect(coreProcObj, SIGNAL(finished()), procThread, SLOT(quit()));
    //delete after finishing
    connect(coreProcObj, SIGNAL(finished()), coreProcObj, SLOT(deleteLater()));
    connect(procThread, SIGNAL(finished()), procThread, SLOT(deleteLater()));

    this->VtwamProcessing = true;
    this->procThread->start();

    timer.start();
}

void dataProcessor::startProcThread(short filtType ,short krnlRadius, short **Input,short **Output,int x_length,int y_length)
{
    int threadNum = 0,startFrame,endFrame,framesPerThread;
    qDebug()<<"Ideal Thread count:"<<procThread->idealThreadCount();//procThread2->idealThreadCount();
    this->threadFinishedCount = 0;

    if (this->boostSpeedEn)
        this->totalThreads = procThread->idealThreadCount();
    else
        this->totalThreads = 1;

    framesPerThread = SAMPLESPERPOINT/this->totalThreads;

    coreProcObjArr  = new coreProcessor* [this->totalThreads];
    procThreadArr   = new QThread* [this->totalThreads];

    for (threadNum = 0;threadNum<this->totalThreads;threadNum++)
    {
        startFrame = framesPerThread*threadNum;
        endFrame = startFrame + framesPerThread;

        this->coreProcObjArr[threadNum] = new coreProcessor(filtType,krnlRadius, Input,Output,x_length,y_length,startFrame,endFrame) ;
        this->procThreadArr[threadNum] = new QThread;

        this->coreProcObjArr[threadNum]->moveToThread(procThreadArr[threadNum]);


        if (threadNum == 0) // only first thread updates the progress
            connect(coreProcObjArr[threadNum], SIGNAL(updateProgress(int,short,short)), this, SLOT(updateProcProgress(int,short,short)));

        connect(coreProcObjArr[threadNum], SIGNAL(finished()), this, SLOT(procFinishedGather()));
        //start process
        connect(procThreadArr[threadNum], SIGNAL(started()), coreProcObjArr[threadNum], SLOT(process()));
        //quit thread
        connect(coreProcObjArr[threadNum], SIGNAL(finished()), procThreadArr[threadNum], SLOT(quit()));
        //delete after finishing
        connect(coreProcObjArr[threadNum], SIGNAL(finished()), coreProcObjArr[threadNum], SLOT(deleteLater()));
        connect(procThreadArr[threadNum], SIGNAL(finished()), procThreadArr[threadNum], SLOT(deleteLater()));
        this->procThreadArr[threadNum]->start();
    }

    timer.start();
}

void dataProcessor::setBoostSpeedEn(bool val)
{
    this->boostSpeedEn = val;
}

void dataProcessor::updateProcProgress(int curframeNum,short FilterRadius, short filterType)
{
    QString StatusTip;
    if (VtwamProcessing == true)
    {
        emit updateProgressBar_dataProcessorSignal( ( (double)curframeNum/buffScanPoints)*100 );
        QTextStream(&StatusTip)<<"Post-processing (VTWAM ) - Processed "<<curframeNum<<"/"<<this->buffScanPoints<<"points";
        emit updateStatusBar_dataProcessorSignal(StatusTip);
    }
    else
    {
        short fltSize = (FilterRadius<<1)+1;
        emit updateProgressBar_dataProcessorSignal( ( (double)(curframeNum*this->totalThreads)/SAMPLESPERPOINT)*100 );

        if (filterType == 1)
            QTextStream(&StatusTip)<<"Post-processing(Median filter "<<fltSize<<"x"<<fltSize<<")- Processed "<<curframeNum*this->totalThreads<<"/"<<SAMPLESPERPOINT <<"frames";
        if (filterType == 2)
            QTextStream(&StatusTip)<<"Post-processing(Spatial Avg filter "<<fltSize<<"x"<<fltSize<<")- Processed "<<curframeNum*this->totalThreads<<"/"<<SAMPLESPERPOINT <<"frames";
        emit updateStatusBar_dataProcessorSignal(StatusTip);
        //we can switch the display buff index here as well
    }
}

void dataProcessor::procFinishedGather()
{
    threadFinishedCount++;

    if (threadFinishedCount == totalThreads)
    {
        threadFinishedCount = 0;
        procFinished();
    }
}

void dataProcessor::procFinished()
{
    bool processAgain=false;
    if (VtwamProcessing == true)
    {
        QString StatusTip;
        emit updateProgressBar_dataProcessorSignal( (double)100 );
        QTextStream(&StatusTip)<<"Post-processing (VTWAM ) - Processed "<<this->buffScanPoints<<"/"<<this->buffScanPoints<<"points";
        emit updateStatusBar_dataProcessorSignal(StatusTip);

        this->VtwamProcessing = false;

        qwtSpectrogram2->updateData(this->vtwamResult,this->VTWAMTitle, 1);
        qwtSpectrogram2->updateAxisXY();

        qDebug()<<"dataProcessor::procFinished() - VTWAM Time taken:"<<timer.elapsed();
    }
    else
    {
        postProcDone = true;
        selectDisplayBuffer(true);
        //dispBuffIndex = (dispBuffIndex+1) % DATABUFFNUM; // advance the dispplay buff index
        setframeNum(frameNum);
        this->filterItr++;
        qDebug()<<"dataProcessor::procFinished()"<<"filterType" <<resultInfoPtr->filterType<<"filterRadius"<<resultInfoPtr->filterRadius<<"filterItr"
               <<filterItr<<"Time taken:"<<timer.elapsed();
        //dont trigger if default process just finished
        if (this->defaultFilter)
        {
            //dispBuffIndex = 1;
            if(this->filterItr  == 1)
                processAgain = true;
            else if(this->subbandDecomp == true && this->filterItr  == 2 && bufToProcess<NUMOFBANDS)
            {
                this->filterItr = 0;
                processAgain = true;
                bufToProcess++;
            }

            if(processAgain)
            {//one more iteration on the same buffer
                int x_length       = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
                int y_length       = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;

                startProcThread(1,1,dataBundle[bufToProcess],dataBundle[bufToProcess],x_length,y_length);
                qDebug()<<"bufToProcess"<<bufToProcess<<"filterItr"<<filterItr;
            }
            else
            {
                this->dataBundleSaved[chosenSubband] = false; // this can now be saved if requested
                this->defaultFilter= false;
                //dispBuffIndex = chosenSubband;
            }
        }
        else
        {
            short FilterBuffNum;
            if((daqInfoPtr->subbandDecomp))
                FilterBuffNum = (chosenSubband+1)%(NUMOFBANDS);
            else
                FilterBuffNum = 1;

            int x_length       = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
            int y_length       = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;

            this->dataBundleSaved[FilterBuffNum] = false; // this may now be saved if requested
            dispBuffIndex = FilterBuffNum;

            qDebug()<<"dataProcessor::procFinished - FilterBuffNum "<<FilterBuffNum;

            if(this->filterItr < resultInfoPtr->filterItr)
            {
                startProcThread(resultInfoPtr->filterType,resultInfoPtr->filterRadius,
                               dataBundle[FilterBuffNum],dataBundle[FilterBuffNum],x_length,y_length);

            }
            else
            { //filtering finished so save the applied filter stats in class
                // do at end of processing ??
                this->filtPass1en  = resultInfoPtr->filtPass1en;
                this->filterType   = resultInfoPtr->filterType;
                this->filterRadius = resultInfoPtr->filterRadius;
            }

            if (resultInfoPtr->filtPass2en)
            {
                if(this->filterItr2 < resultInfoPtr->filterItr2)
                {
                    //PASS1 - filtering finished so save the applied filter stats in class
                    this->filtPass1en  = resultInfoPtr->filtPass1en;
                    this->filterType   = resultInfoPtr->filterType;
                    this->filterRadius = resultInfoPtr->filterRadius;

                    startProcThread(resultInfoPtr->filterType2,resultInfoPtr->filterRadius2,
                                   dataBundle[FilterBuffNum],dataBundle[FilterBuffNum],x_length,y_length);

                    this->filterItr2++;
                }
                else
                { //filtering finished so save the applied filter stats in class
                    // do at end of processing ??
                    this->filtPass2en  = resultInfoPtr->filtPass2en;
                    this->filterType2   = resultInfoPtr->filterType2;
                    this->filterRadius2 = resultInfoPtr->filterRadius2;
                }
            }

        }
    }
}

void dataProcessor::loadData(bool status)
{
    timer.start();
    QFile myfilein;
    QString filepath,infolderpath;
    QString StatusTip;
    qint64 result;

    unsigned int buffScanPointsLoc;
    float buffScanHeightLoc;
    float buffScanWidthLoc;
    float buffScanIntervalLoc;
    bool readEnableTT = false;
    int SamplingFreq;

    if(dataBundle !=NULL && dataBundleSaved[0] == false)
    {
//        QMessageBox(Icon icon, const QString &title, const QString &text,
//                    StandardButtons buttons = NoButton, QWidget *parent = 0,
//                    Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

        int result;
        QMessageBox msgBox(QMessageBox::Question, tr("Scan Data"),tr("Current UWPI data has not been saved. Do you want to overwrite this data ?"),
                           QMessageBox::Yes|QMessageBox::No);
        result = msgBox.exec();

        if (result ==QMessageBox::No)
        return;
    }


    QFileDialog *fd = new QFileDialog;
    //QTreeView *tree = fd->findChild <QTreeView*>();
    //tree->setRootIsDecorated(true);
    //tree->setItemsExpandable(true);
    fd->setFileMode(QFileDialog::Directory);
    fd->setOption(QFileDialog::ShowDirsOnly);
    fd->setViewMode(QFileDialog::Detail);
    fd->setDirectory(progDataPath+"\\Data");

    if (fd->exec())
    {
        infolderpath = fd->selectedFiles()[0];
        qDebug()<<filepath;

        //save the data related settings necessary for loading the data next time.
         filepath = infolderpath+"\\Setting(DoNotEdit).bin";
         myfilein.setFileName(filepath);
         if(!myfilein.open(QIODevice::ReadOnly))
         {
             QMessageBox msgBox(QMessageBox::Critical, tr("File Error"),tr("Could'nt open parameter file for data."), 0);
             msgBox.exec();
             return;
         }
         QDataStream in(&myfilein);
         in.setByteOrder(QDataStream::LittleEndian);

         in>>buffScanPointsLoc;
         in>>buffScanHeightLoc;
         in>>buffScanWidthLoc;
         in>>buffScanIntervalLoc;
 		 in>>SamplingFreq;

         in>>readEnableTT;

         myfilein.close();

         daqInfoPtr->ScanPoints = buffScanPointsLoc;
         scanInfoPtr->scanWidth = buffScanWidthLoc  ;
         scanInfoPtr->scanHeight = buffScanHeightLoc ;
         scanInfoPtr->scanInterval = buffScanIntervalLoc;
		 daqInfoPtr->SamplingFreq  = SamplingFreq;
         daqInfoPtr->subbandDecomp = false;
          scanInfoPtr->useCurrentResults = false;
         scanInfoPtr->enableTT  =   readEnableTT;

         qDebug()<<"dataProcessor::loadData"<<
                   "buffScanWidthLoc "<<buffScanWidthLoc<<
                   "buffScanHeightLoc"<< buffScanHeightLoc<<
                   "buffScanIntervalLoc"<< buffScanIntervalLoc<<
                   "buffScanPointsLoc"<<buffScanPointsLoc<<
                   "dispBuffIndex"<<this->dispBuffIndex<<
                   "readEnableTT"<<readEnableTT;

         if (!allocateMem())
            return;
         for (int fnum = 0; fnum<SAMPLESPERPOINT; fnum++)
         {
             filepath = infolderpath+"\\"+QString::number(fnum)+".bin";

             myfilein.setFileName(filepath);
             if(!myfilein.open(QIODevice::ReadOnly))
             {
                 QMessageBox msgBox(QMessageBox::Critical, tr("File Error"),tr("Could'nt open data file."), 0);
                 msgBox.exec();
                 return;
             }
             /*
             QDataStream in(&myfilein);
             in.setByteOrder(QDataStream::LittleEndian);

             for (unsigned int i=0; i<buffScanPoints; i++)
                in>>dataBundle[0][fnum][i];
             */

             result=myfilein.read((char*)&dataBundle[0][fnum][0],2*buffScanPoints);


             myfilein.close();

             //update current status
             emit updateProgressBar_dataProcessorSignal( ( (double)(fnum+1)/SAMPLESPERPOINT)*100 );
             StatusTip.clear();
             QTextStream(&StatusTip)<<"Loading data frame: "<<fnum+1<<"/"<<SAMPLESPERPOINT;
             emit updateStatusBar_dataProcessorSignal(StatusTip);

             QCoreApplication::processEvents();

             if (fnum==0)
             {
                 dataBundleSaved[0] = true;
                 chosenSubband = 0;
                 dispBuffIndex = 0;
                 setframeNum(0);
             }
         }
         qDebug()<<"File Reading Time: (msec)"<<timer.elapsed();
    }
}
void dataProcessor::saveData(bool status)
{
    timer.start();

    QFile myfileout;
    QString filepath;
    QString StatusTip;
    qint64 result;

    if (dataBundle == NULL)
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Data Error"),tr("No data available for saving."), 0);
        msgBox.exec();
        return;
    }

    if (dataBundle[dispBuffIndex] == NULL)
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Data Error"),tr("No data available for saving."), 0);
        msgBox.exec();
        return;
    }

    if(dataBundle[dispBuffIndex][frameNum] == NULL)
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Data Error"),tr("No data available for saving."), 0);
        msgBox.exec();
        return;
    }

   if (dataBundleSaved[dispBuffIndex] == TRUE)
   {
       QMessageBox msgBox(QMessageBox::Information, tr("Data Saved"),tr("This data was already saved."), 0);
       msgBox.exec();
       qDebug()<<"dataProcessor::saveData - This data was already saved, dispBuffIndex: " << dispBuffIndex;
       return;
   }

   QDateTime now = QDateTime::currentDateTime();
   QString dataStoragePath = this->progDataPath + "\\Data\\Data"+now.toString("ddMMyy_hhmmss")+"\\";
   if( QDir(dataStoragePath).exists() == false)
       QDir().mkpath(dataStoragePath);
   //save the data related settings necessary for loading the data next time.

   filepath = dataStoragePath+"Setting(DoNotEdit).bin";
    myfileout.setFileName(filepath);
    if(!myfileout.open(QIODevice::WriteOnly))
    {
        QMessageBox errBox;
        errBox.setText("Could not open file for writing.");
        errBox.exec();
        return;
    }
    QDataStream out(&myfileout);
    out.setByteOrder(QDataStream::LittleEndian);

    out<<buffScanPoints;
    out<<buffScanHeight;
    out<<buffScanWidth;
    out<<buffScanInterval;
    out<<daqInfoPtr->SamplingFreq;
    out<<scanInfoPtr->enableTT;

    myfileout.close();

    for (int fnum = 0; fnum<SAMPLESPERPOINT; fnum++)
    {
        filepath = dataStoragePath+QString::number(fnum)+".bin";

        myfileout.setFileName(filepath);
        if(!myfileout.open(QIODevice::WriteOnly))
        {
            QMessageBox errBox;
            errBox.setText("Could not open file for writing.");
            errBox.exec();
            return;
        }
        /*
        QDataStream out(&myfileout);
        out.setByteOrder(QDataStream::LittleEndian);

        for (unsigned int i=0; i<buffScanPoints; i++)
           out<<dataBundle[dispBuffIndex][fnum][i];
        */

        result=myfileout.write((char *)&dataBundle[0][fnum][0],2*buffScanPoints);

        myfileout.close();

        //update current status
        emit updateProgressBar_dataProcessorSignal( ( (double)(fnum+1)/SAMPLESPERPOINT)*100 );
        StatusTip.clear();
        QTextStream(&StatusTip)<<"Saving data frame: "<<fnum+1<<"/"<<SAMPLESPERPOINT;
        emit updateStatusBar_dataProcessorSignal(StatusTip);

        QCoreApplication::processEvents();
    }
    dataBundleSaved[dispBuffIndex] = true;

    qDebug()<<"Filewriting Time: (msec)"<<timer.elapsed()<<"buffScanPoints"<<this->buffScanPoints<<"dispBuffIndex"<<this->dispBuffIndex;
}

void dataProcessor::loadDataComp(bool status)
{
    timer.start();

    QFile myfilein;
    QString filepath,infolderpath;
    QString StatusTip;
    QByteArray frameByteArrayComp,frameByteArrayUnComp;

    if(dataBundle !=NULL && dataBundleSaved[1] == false)
    {
//        QMessageBox(Icon icon, const QString &title, const QString &text,
//                    StandardButtons buttons = NoButton, QWidget *parent = 0,
//                    Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

        int result;
        QMessageBox msgBox(QMessageBox::Question, tr("Scan Data"),tr("Current UWPI data has not been saved. Do you want to overwrite this data ?"),
                           QMessageBox::Yes|QMessageBox::No);
        result = msgBox.exec();

        if (result ==QMessageBox::No)
        return;
    }


    QFileDialog *fd = new QFileDialog;
    //QTreeView *tree = fd->findChild <QTreeView*>();
    //tree->setRootIsDecorated(true);
    //tree->setItemsExpandable(true);
    fd->setFileMode(QFileDialog::Directory);
    fd->setOption(QFileDialog::ShowDirsOnly);
    fd->setViewMode(QFileDialog::Detail);
    fd->setDirectory(progDataPath+"\\Data");

    if (fd->exec())
    {
        infolderpath = fd->selectedFiles()[0];
        qDebug()<<filepath;

        //save the data related settings necessary for loading the data next time.
         filepath = infolderpath+"\\Setting(DoNotEdit).bin";
         myfilein.setFileName(filepath);
         if(!myfilein.open(QIODevice::ReadOnly))
         {
             QMessageBox msgBox(QMessageBox::Critical, tr("File Error"),tr("Could'nt open parameter file for data."), 0);
             msgBox.exec();
             return;
         }
         QDataStream in(&myfilein);
         in.setByteOrder(QDataStream::LittleEndian);

         in>>buffScanPoints;
         in>>buffScanHeight;
         in>>buffScanWidth;
         in>>buffScanInterval;
         myfilein.close();

         daqInfoPtr->ScanPoints = buffScanPoints;
         scanInfoPtr->scanWidth = buffScanWidth  ;
         scanInfoPtr->scanHeight = buffScanHeight ;
         scanInfoPtr->scanInterval = buffScanInterval;

         if (!allocateMem())
            return;
         for (int fnum = 0; fnum<SAMPLESPERPOINT; fnum++)
         {
             filepath = infolderpath+"\\"+QString::number(fnum)+".bin";

             myfilein.setFileName(filepath);
             if(!myfilein.open(QIODevice::ReadOnly))
             {
                 QMessageBox msgBox(QMessageBox::Critical, tr("File Error"),tr("Could'nt open data file."), 0);
                 msgBox.exec();
                 return;
             }

             //frameByteArrayUnComp.setRawData((const char *)&dataBundle[1][fnum][0],2*buffScanPoints);
             frameByteArrayComp = myfilein.readAll();
             frameByteArrayUnComp = qUncompress(frameByteArrayComp);
             char *tempPointer = (char *)&dataBundle[1][fnum][0];
             for (unsigned int temp = 0; temp<(2*buffScanPoints);temp++)
                 tempPointer[temp] = frameByteArrayUnComp.at(temp);

             /*
             QDataStream in(&myfilein);
             in.setByteOrder(QDataStream::LittleEndian);

             for (int i=0; i<buffScanPoints; i++)
                in>>dataBundle[1][fnum][i];
*/
             myfilein.close();

             //update current status
             emit updateProgressBar_dataProcessorSignal( ( (double)(fnum+1)/SAMPLESPERPOINT)*100 );
             StatusTip.clear();
             QTextStream(&StatusTip)<<"Loading data frame: "<<fnum+1<<"/"<<SAMPLESPERPOINT;
             emit updateStatusBar_dataProcessorSignal(StatusTip);

             QCoreApplication::processEvents();
         }
         dataBundleSaved[1] = false;
         dispBuffIndex = 1;
         setframeNum(0);
         qDebug()<<"File Reading Time: (msec)"<<timer.elapsed()<<"buffScanPoints"<<this->buffScanPoints<<"dispBuffIndex"<<this->dispBuffIndex;
    }

}

void dataProcessor::saveDataComp(bool status)
{
    timer.start();

    QFile myfileout;
    QString filepath;
    QString StatusTip;

    QByteArray frameByteArray,frameByteArrayComp;

    if (dataBundle == NULL)
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Data Error"),tr("No data available for saving."), 0);
        msgBox.exec();
        return;
    }

    if (dataBundle[dispBuffIndex] == NULL)
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Data Error"),tr("No data available for saving."), 0);
        msgBox.exec();
        return;
    }

   if(dataBundle[dispBuffIndex][frameNum] == NULL)
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Data Error"),tr("No data available for saving."), 0);
        msgBox.exec();
        return;
    }

   if (dataBundleSaved[dispBuffIndex] == TRUE)
   {
       QMessageBox msgBox(QMessageBox::Information, tr("Data Saved"),tr("This data was already saved."), 0);
       msgBox.exec();
       return;
   }


   QDateTime now = QDateTime::currentDateTime();
   QString dataStoragePath = this->progDataPath + "\\Data\\Data"+now.toString("ddMMyy_hhmmss")+"\\";
   if( QDir(dataStoragePath).exists() == false)
       QDir().mkpath(dataStoragePath);
   //save the data related settings necessary for loading the data next time.

   filepath = dataStoragePath+"Setting(DoNotEdit).bin";
    myfileout.setFileName(filepath);
    if(!myfileout.open(QIODevice::WriteOnly))
    {
        QMessageBox errBox;
        errBox.setText("Could not open file for writing.");
        errBox.exec();
        return;
    }
    QDataStream out(&myfileout);
    out.setByteOrder(QDataStream::LittleEndian);

    out<<buffScanPoints;
    out<<buffScanHeight;
    out<<buffScanWidth;
    out<<buffScanInterval;
    myfileout.close();


    for (int fnum = 0; fnum<SAMPLESPERPOINT; fnum++)
    {
        filepath = dataStoragePath+QString::number(fnum)+".bin";
        myfileout.setFileName(filepath);
        if(!myfileout.open(QIODevice::WriteOnly))
        {
            QMessageBox errBox;
            errBox.setText("Could not open file for writing.");
            errBox.exec();
            return;
        }

        //how to set endian for writing?
        frameByteArray.setRawData((const char *)&dataBundle[dispBuffIndex][fnum][0],2*buffScanPoints);
        frameByteArrayComp = qCompress(frameByteArray,9);
        myfileout.write(frameByteArrayComp);

        //update current status
        emit updateProgressBar_dataProcessorSignal( ( (double)(fnum+1)/SAMPLESPERPOINT)*100 );
        StatusTip.clear();
        QTextStream(&StatusTip)<<"Saving data frame: "<<fnum+1<<"/"<<SAMPLESPERPOINT;
        emit updateStatusBar_dataProcessorSignal(StatusTip);
        myfileout.close();
        QCoreApplication::processEvents();
    }


    dataBundleSaved[dispBuffIndex] = true;

    qDebug()<<"Filewriting Time: (msec)"<<timer.elapsed()<<"buffScanPoints"<<this->buffScanPoints<<"dispBuffIndex"<<this->dispBuffIndex;
}

bool dataProcessor::alloc2dArray16bit(int rows,int columns,bool allocRptScanBuffOnly)
{ 
   timer.start();
   short dataBufnum = 2;
   if(allocRptScanBuffOnly == false)
   {
       if (daqInfoPtr->subbandDecomp == true)
           dataBufnum = NUMOFBANDS;

       if ( (dataBundle = (short ***)calloc(dataBufnum,sizeof(short **))) == NULL)
            return 0;

       for (int i = 0;i<dataBufnum;i++)
       {
           if ( (dataBundle[i] = (short **)calloc(rows,sizeof(short *))) == NULL)
                return 0;

            for (int j = 0; j<rows; j++)
            {
                if( (dataBundle[i][j] = (short *)calloc(columns,sizeof (short))) == NULL)
                    return 0;
            }
       }
   }

   //allocate the rpt scan accumulator buffer

   if (scanInfoPtr->scansPerInspection > 1 || scanInfoPtr->useCurrentResults)
   {
       if ( (rptScanAccDataBundle = (int **)calloc(rows,sizeof(int *))) == NULL)
            return 0;

       for (int j = 0; j<rows; j++)
       {
           if( (rptScanAccDataBundle[j] = (int *)calloc(columns,sizeof (int))) == NULL)
               return 0;
       }
   }

   qDebug()<<timer.elapsed()<<"(msec) to declare the dataBuffer";

#ifndef ACTUALSYSTEM
   if(allocRptScanBuffOnly == false)
   {
       timer.start();
       for (unsigned int i = 0;i<dataBufnum;i++)
       {
           for (unsigned int j = 0;j < SAMPLESPERPOINT;j++)
           {
               for (unsigned int k = 0;k<daqInfoPtr->ScanPoints;k++)
               {
                   //dataBundle[i][j][k] = 10*j+ 100*i+k;
                   //dataBundle[i][j][k] = 100*k;
                   dataBundle[i][j][k] = -100;
               }
           }
       }
       qDebug()<<timer.elapsed()<<"(msec) to init the dataBuffer";
   }
#endif

   qDebug()<<"dataProcessor::alloc2dArray16bit.";
   return 1;
}

bool dataProcessor::clear2dArray16bit(int rows,int columns,bool clearRptScanBuffOnly)
{
    if(clearRptScanBuffOnly == false)
    {
        short dataBufnum = 2;
        if (subbandDecomp == true)
            dataBufnum = NUMOFBANDS;

        if (dataBundle == NULL)
        {
            qDebug()<<"Error: clear2dArray16bit - dataBundle is already free";
            return 0;
        }
        for (int i = 0; i<dataBufnum; i++)
        {
            for (int j = 0; j<rows; j++)
            {
                if(dataBundle[i][j] == NULL)
                {
                    qDebug()<<"Error: clear2dArray16bit - dataBundle[i][j] is already free"<< i<<j;
                    break; //since allocation was sequential, no more rows allocated
                }
                free(dataBundle[i][j]);
                dataBundle[i][j] = NULL;
            }
            free(dataBundle[i]);
            dataBundle[i] = NULL;
        }
        free(dataBundle);
        dataBundle = NULL;
    }

    //clear rpt scan buffer if required
    if (rptScansPerInspection > 1 || useCurrentResults)
    {
        for (int j = 0; j<rows; j++)
        {
            if(rptScanAccDataBundle[j] == NULL)
            {
                qDebug()<<"Error: clear2dArray16bit - rptScanAccDataBundle[j] is already free"<<j;
                break; //since allocation was sequential, no more rows allocated
            }
            else
            {
                free(rptScanAccDataBundle[j]);
                rptScanAccDataBundle[j] = NULL;
            }
        }
        free(rptScanAccDataBundle);
        rptScanAccDataBundle = NULL;
    }

    qDebug()<<"dataProcessor::clear2dArray16bit.";

    return 1;
}
bool dataProcessor::isMemValid(short*** dataBundle, int dispBuffIndex, int frameNum ,const char* callername,bool isSilent)
{
    bool return_val = false;
    QString ErrorMsg;
    if (dataBundle!=NULL)
    {
        if (dataBundle[dispBuffIndex] != NULL)
        {
            if(dataBundle[dispBuffIndex][frameNum] != NULL)
                return_val =  true;
            else
                ErrorMsg = QString(callername) +  " - dataBundle[" + QString::number(dispBuffIndex)+"]"+"["+QString::number(frameNum)+"] = NULL";

        }
        else
            ErrorMsg = QString(callername) +  " - dataBundle[" + QString::number(dispBuffIndex)+"] = NULL";
    }
    else
        ErrorMsg = QString(callername) +  " - dataBundle = NULL";

    if (return_val==false)
    {
        qDebug()<<ErrorMsg;
        if (isSilent == false)
        {
            QMessageBox msgBox(QMessageBox::Critical, tr("Memory Error"),ErrorMsg, 0);
            msgBox.exec();
        }
    }

    return return_val;
}


inline short dataProcessor::IIRfilterB1(short in,bool reset)
{
   static double inAcc,outAcc;
   static double buf1,buf2,buf3,buf4;

   if (reset == true) // reset the state vars when new filtering starts
       inAcc=outAcc=buf1=buf2=buf3=buf4=0.0;

    //200-500
    double a1 = 1, a2 = -3.95340937166299, a3 = 5.86341125073283, a4 = -3.86654437944130, a5 = 0.956543676511204;
    double b1 = 0.000241359048974918,	b2=0.0,	b3=-0.000482718097949836,	b4=0.0,	b5=0.000241359048974918;

    //10k~90k
    //double a1 = 1, a2 = -3.98813276868391, a3 =	5.96448787337403, a4 = 	-3.96457732426005, a5 =	0.988222219666761;
    //double b1 = 1.74425378099272e-05,b2 = 0.0, b3 = -3.48850756198545e-05, b4 = 0.0,b5 = 1.74425378099272e-05;


    //input acc for the IIR
    inAcc = in;
    inAcc = inAcc - (a2*buf1);
    inAcc = inAcc - (a3*buf2);
    inAcc = inAcc - (a4*buf3);
    inAcc = inAcc - (a5*buf4);

    //output acc for the IIR (FIR part in IIR)
    outAcc = b1*inAcc;
    outAcc = outAcc + (b2*buf1);
    outAcc = outAcc + (b3*buf2);
    outAcc = outAcc + (b4*buf3);
    outAcc = outAcc + (b5*buf4);

    buf4 = buf3;
    buf3 = buf2;
    buf2 = buf1;
    buf1 = inAcc;

    return(round(outAcc));
}

//DF2 filter
inline short dataProcessor::IIRfilterB2(short in,bool reset)
{
 //optimization for speed if necessary
   static double inAcc,outAcc;
   static double buf1,buf2,buf3,buf4;

   if (reset == true) // reset the state vars when new filtering starts
       inAcc=outAcc=buf1=buf2=buf3=buf4=0.0;

    //500 - 1000
    double a1 = 1,	a2 = -3.91522312056265, a3=5.75949354217813,	a4=-3.77286854920398,	a5=0.928627086124806;
    double b1=0.000660779098216284,	b2=0.0,	b3=-0.00132155819643257,	b4=0.0,	b5=0.000660779098216284;

    //90k~170k
    //double a1=1,	a2 = -3.98781787960825, a3=5.96385998296624, a4 = -3.96426429504025, a5 = 0.988222219666761;
    //double b1=1.74425359223657e-05,	b2 = 0.0	,b3=-3.48850718447313e-05, b4=0, b5 =	1.74425359223657e-05;

    //input acc for the IIR
    inAcc = in;
    inAcc = inAcc - (a2*buf1);
    inAcc = inAcc - (a3*buf2);
    inAcc = inAcc - (a4*buf3);
    inAcc = inAcc - (a5*buf4);

    //output acc for the IIR (FIR part in IIR)
    outAcc = b1*inAcc;
    outAcc = outAcc + (b2*buf1);
    outAcc = outAcc + (b3*buf2);
    outAcc = outAcc + (b4*buf3);
    outAcc = outAcc + (b5*buf4);

    buf4 = buf3;
    buf3 = buf2;
    buf2 = buf1;
    buf1 = inAcc;

    return(round(outAcc));
}
//DF2 filter
inline short dataProcessor::IIRfilterB3(short in,bool reset)
{
 //optimization for speed if necessary
   static double inAcc,outAcc;
   static double buf1,buf2,buf3,buf4;

   if (reset == true) // reset the state vars when new filtering starts
       inAcc=outAcc=buf1=buf2=buf3=buf4=0.0;

    //1000-1300
    double a1=1,	a2=-3.92741385494988,	a3=5.81218855122669,	a4=-3.84112004070264,	a5=0.956543676511202;
    double b1=0.000241359049046793,	b2=0,	b3=-0.000482718098093586,	b4=0.0,	b5=0.000241359049046793;

    //170k~250k
    //double a1 = 1,	a2=-3.98722311174359, a3=	5.96267414809902, a4=	-3.96367304010313, a5=	0.988222219666764;
    //double b1 = 1.74425347968775e-05,  b2=	0,	b3=-3.48850695937549e-05, b4=	0, b5=	1.74425347968775e-05;


    //input acc for the IIR
    inAcc = in;
    inAcc = inAcc - (a2*buf1);
    inAcc = inAcc - (a3*buf2);
    inAcc = inAcc - (a4*buf3);
    inAcc = inAcc - (a5*buf4);

    //output acc for the IIR (FIR part in IIR)
    outAcc = b1*inAcc;
    outAcc = outAcc + (b2*buf1);
    outAcc = outAcc + (b3*buf2);
    outAcc = outAcc + (b4*buf3);
    outAcc = outAcc + (b5*buf4);

    buf4 = buf3;
    buf3 = buf2;
    buf2 = buf1;
    buf1 = inAcc;

    return(round(outAcc));
}
//FiltType = 1: median filter
//FiltType = 2: Spatial filter
//KrnlSize = 3 5 7 -> //KrnlRadius = 1 2 3

coreProcessor::coreProcessor(char filtType, char krnlRadius, short **input, short **output,
                             int x_length, int y_length,int startFrame, int endFrame,unsigned int buffScanPoints,unsigned int * vtwamResult,
                             structResult *resultInfoPtr_arg)
{
    this->filtType    =  filtType;
    this->krnlRadius  =  krnlRadius;
    this->input       =  input;
    this->output      =  output;
    this->x_length    =  x_length;
    this->y_length    =  y_length;
    this->startFrame  =  startFrame;
    this->endFrame    =  endFrame;
    this->buffScanPoints = buffScanPoints;
    this->vtwamResult   = vtwamResult;
    this->wvfrmAcquired = -1;
    this->abortThread   = false;
    this->resultInfoPtr = resultInfoPtr_arg;
}

coreProcessor::~coreProcessor()
{

}

void coreProcessor::setAbortThread()
{
    this->abortThread = true;
}

void coreProcessor::updateRowColNumAcquired(int wvfrmAcquiredArg)
{
    wvfrmAcquired = wvfrmAcquiredArg;
    //qDebug()<<"coreProcessor::updateRowColNumAcquired" <<wvfrmAcquired;
}

void coreProcessor::processParallel()
{
    int x,y,top,bottom,left,right,yKernel,xKernel,kernelDataIndex,frameNum,i,avgSum,j;
    int wvfrmProcessed = -1;
    int y_start=0,x_start=0,y_end=0,x_end=0;
    short value;
    int reqSignalsAfterCntr = (krnlRadius*x_length)+1;
    short kernelData[50]; // since max kernel size is 7x7
    int pivot = (x_length-1)>>1;
    int xToUse;

    //while( wvfrmProcessed < ((x_length*y_length)-1) ) //start counting the waveforms from zero
    while( wvfrmProcessed < ((x_length*y_length)-1) - (reqSignalsAfterCntr) && abortThread==false )
    {

//        qDebug()<<"coreProcessor::processParallel() - BeforeLoop"
//                <<"wvfrmAcquired"<<wvfrmAcquired
//                <<"((krnlRadius*x_length)+1) aka reqSignalsAfterCntr"<<reqSignalsAfterCntr
//               <<"wvfrmProcessed"<<wvfrmProcessed
//              <<"(wvfrmAcquired - ((krnlRadius*x_length)+1))"<<(wvfrmAcquired - reqSignalsAfterCntr);

        while( (wvfrmAcquired < ((krnlRadius*x_length)+1) ||
               wvfrmProcessed == (wvfrmAcquired - reqSignalsAfterCntr)) &&
               (abortThread  == false))
        {
            QCoreApplication::processEvents();
        }

        //just for debug print
       x_start = ((wvfrmProcessed+1) % x_length );
       x_end = ( (wvfrmAcquired - reqSignalsAfterCntr) % x_length );

       y_start = ((wvfrmProcessed+1) / x_length );
       y_end = ( (wvfrmAcquired - reqSignalsAfterCntr) / x_length );
        /*
       qDebug()<<"After Loop - coreProcessor::processParallel() - "
              <<"wvfrmProcessed"<<wvfrmProcessed<<"x_start"<<x_start<<"y_start"<<y_start
              <<"wvfrmAcquired"<<wvfrmAcquired<<"x_end"<<x_end<<"y_end"<<y_end
              <<"wvfrmAcquired - ((krnlRadius*x_length)+1)"<<wvfrmAcquired - reqSignalsAfterCntr
             <<"pivot"<<pivot;
        */

       for (frameNum = startFrame;frameNum<endFrame;frameNum++)
       {
           for (y = y_start; y <= y_end; y++)
           {
               //top     = MAX(y-krnlRadius,0);
               //bottom  = MIN(y+krnlRadius,y_length-1);
               top     = y-krnlRadius;
               bottom  = y+krnlRadius;
               if (y_start<y_end) //so two rows should be visited
               {
                   if (y == y_start)
                   {
                       x_start = ((wvfrmProcessed+1) % x_length );
                       x_end   = (x_length-1);
                   }
                   else if (y == y_end)
                   {
                       x_start = 0;
                       x_end   = ( (wvfrmAcquired - reqSignalsAfterCntr) % x_length );
                   }
                   else
                   {
                       x_start =    0;
                       x_end   =   (x_length-1);
                   }
               }
               for (x = x_start; x <= x_end; x++)
               {
                   if (y%2 != 0)
                       xToUse = (pivot-x)+pivot; //x_length-1 because the count starts from '0'
                   else
                       xToUse = x;

                   if ( (y < krnlRadius) ||
                        (y >= (y_length-krnlRadius)) ||
                        (xToUse < krnlRadius) ||
                        (xToUse >= (x_length-krnlRadius)) ) // for border pixels just copy without applying the filter
                   {
                       //no copy necessary in case of same buffer
                       if(output!=input)
                           output[frameNum][xToUse+y*x_length] = input[frameNum][xToUse+y*x_length];
                        /*
                        if (frameNum==100)
                        qDebug()<<"simple copy"<<"frameNum"<<frameNum<<"y: "<<y<<"xToUse: "<<xToUse<<"x:"<<x;
                       */
                       continue;
                   }

                   left    = xToUse-krnlRadius;
                   right   = xToUse+krnlRadius;

                   kernelDataIndex = 0;
                   for (yKernel = top; yKernel<=bottom; yKernel++)
                   {
                       for (xKernel = left; xKernel<=right; xKernel++)
                       {
                           kernelData[kernelDataIndex++] = input[frameNum][xKernel+yKernel*x_length];
                       }
                   }

                   switch (filtType)
                   {
                   case 1: //median
                           switch(krnlRadius)
                           {
                               case 1: value = optMed9(&kernelData[0]) ;break;
                               case 2: value = optMed25(&kernelData[0]);break;
                               case 3: value = optMed49(&kernelData[0]);break;
                               default: qWarning("coreProcessor::process() - Wrong krnlRadius for median filtering");
                               emit finished();break;
                           }
                       break;

                   case 2: //spatial Avg
                           avgSum = 0;
                           for (i = 0; i<kernelDataIndex;i++)
                               avgSum += kernelData[i];
                           value = avgSum/(kernelDataIndex);
                       break;

                   default:
                       qWarning("coreProcessor::process() - Wrong filtType");
                       emit finished();
                       break;
                   }
                   output[frameNum][xToUse+y*x_length] = value;
                   /*
                   if (frameNum==100)
                       qDebug()<<"main loop"<<"frameNum"<<frameNum<<"y:"<<y<<"xToUse:"<<xToUse<<"x:"<<x;
                   */
               }
           }
/*
           for(j = (wvfrmProcessed+1);j<=(wvfrmAcquired - reqSignalsAfterCntr) ;j++)
           {   int tempDebug;
               tempDebug = output[frameNum][j] = tempFrame[j];
           }
*/
       }
       emit updateProgress((frameNum+1),krnlRadius,filtType); //frame number 1~SAMPLESPERPOINT;

       wvfrmProcessed = wvfrmAcquired - reqSignalsAfterCntr;
       qDebug()<<QThread::currentThread()<<"wvfrmProcessed ="<<wvfrmProcessed<<"wvfrmAcquired ="<<wvfrmAcquired<<"reqSignalsAfterCntr ="<<reqSignalsAfterCntr;
   }

   //free(tempFrame);
   qDebug()<<QThread::currentThread();
   if (input!=output)
   {
       int j_real;
       for (frameNum = startFrame;frameNum<endFrame;frameNum++)
       {
           for(j = wvfrmProcessed+1;j<=wvfrmAcquired; j++)
           {
               if (((j/x_length) % 2) !=0) //odd row
               {
                   j_real = (pivot - (j%x_length))+pivot;
                   y = (j/x_length) * x_length;

                   j_real+=y;

               }
               else
                   j_real=j;

               output[frameNum][j_real] = input[frameNum][j_real];
           }
       }
   }
   emit finished();
}

void coreProcessor::processVtwam()
{
    unsigned int i;
    int j;
    unsigned int sum;
    int startFrame,endFrame;
    int index = 0;

    do
    {
        startFrame  = this->resultInfoPtr->vtwamStartFr[index];
        endFrame    = this->resultInfoPtr->vtwamEndFr[index];


        if (startFrame > endFrame)
            SWAP(startFrame,endFrame);

        for (i=0;i<buffScanPoints;i++)
        {
            if (index == 0)
                sum = 0;
            else
                sum = vtwamResult[i];
            for (j = startFrame; j<=endFrame; j++)
            {
                //absolute sum of the corresponding point in frames start~end
                sum += qAbs(input[j][i]);
                //sum += (input[j][i]);
                //sum += input[j][i]*input[j][i];
            }
            vtwamResult[i] = sum;
            if ((i%500) == 0)
                emit updateProgress((i+1),1,1);//frame number 1~SAMPLESPERPOINT;
        }

        index++;
    }while(index <= this->resultInfoPtr->vtwamRangeNo);

    qDebug()<<QThread::currentThread();
    emit finished();
}


void coreProcessor::process()
{
    int x,y,top,bottom,left,right,yKernel,xKernel,kernelDataIndex,frameNum,i,avgSum,j;
    short value;
    //krnlSz = (krnlRadius*2)+1;

    short kernelData[50]; // since max kernel size is 7x7
    short *tempFrame = NULL;

    if ( (tempFrame = (short *)calloc(x_length*y_length,sizeof(short))) == NULL)
    {
        qWarning("coreProcessor::process() - cant declare tempFrame ");
        emit finished();
    }
    qDebug()<<" coreProcessor::process Started"<<QThread::currentThread()<<
              "startFrame: "<<startFrame<<
              "endFrame"<<endFrame;

    //for (frameNum = 0;frameNum<SAMPLESPERPOINT;frameNum++)
    for (frameNum = startFrame;frameNum<endFrame;frameNum++)
    {
        for (y = 0; y < y_length; y++)
        {
            //top     = MAX(y-krnlRadius,0);
            //bottom  = MIN(y+krnlRadius,y_length-1);
            top     = y-krnlRadius;
            bottom  = y+krnlRadius;

            for (x = 0; x < x_length; x++)
            {
                if ( (y < krnlRadius) ||
                     (y >= (y_length-krnlRadius)) ||
                     (x < krnlRadius) ||
                     (x >= (x_length-krnlRadius)) ) // for border pixels just copy without applying the filter
                {
                     tempFrame[x+y*x_length] = input[frameNum][x+y*x_length];
//                     tempFrame[y+x*y_length] = input[frameNum][y+x*y_length];
                     continue;
                }

                //left    = MAX(x-krnlRadius,0);
                //right   = MIN(x+krnlRadius,x_length);

                left    = x-krnlRadius;
                right   = x+krnlRadius;

                kernelDataIndex = 0;
                for (yKernel = top; yKernel<=bottom; yKernel++)
                {
                    for (xKernel = left; xKernel<=right; xKernel++)
                    {
                        kernelData[kernelDataIndex++] = input[frameNum][xKernel+yKernel*x_length];
//                        kernelData[kernelDataIndex++] = input[frameNum][yKernel+xKernel*y_length];
                    }
                }

                switch (filtType)
                {
                case 1: //median
                        switch(krnlRadius)
                        {
                            case 1: value = optMed9(&kernelData[0]) ;break;
                            case 2: value = optMed25(&kernelData[0]);break;
                            case 3: value = optMed49(&kernelData[0]);break;
                            default: qWarning("coreProcessor::process() - Wrong krnlRadius for median filtering");
                            emit finished();break;
                        }
                    break;

                case 2: //spatial Avg
                        avgSum = 0;
                        for (i = 0; i<kernelDataIndex;i++)
                            avgSum += kernelData[i];
                        value = avgSum/(kernelDataIndex);
                    break;

                default:
                    qWarning("coreProcessor::process() - Wrong filtType");
                    emit finished();
                    break;
                }
                tempFrame[x+y*x_length] = value;
//                tempFrame[y+x*y_length] = value;
            }
        }
        for(j = 0;j<(x_length*y_length) ;j++)
            output[frameNum][j] = tempFrame[j];

        if(startFrame==0)
            emit updateProgress((frameNum+1),krnlRadius,filtType);//frame number 1~SAMPLESPERPOINT;
    }

    free(tempFrame);
    qDebug()<<" coreProcessor::process finishing"<<QThread::currentThread()<<
              "startFrame: "<<startFrame<<
              "endFrame"<<endFrame;

    emit finished();
}

void coreProcessor::processIIR()
{

}

//DF2 filter
inline void coreProcessor::IIRfilter(short* in,short* outBuff, short bandNo)
{
 //optimization for speed if necessary

   double a1,a2,a3,a4,a5,b1,b2,b3,b4,b5;
   double inAcc=0.0,outAcc=0.0;
   double buf1=0.0,buf2=0.0,buf3=0.0,buf4=0.0;
   int i;

    switch (bandNo)
    {

    case 1:
        a1 = 1; a2 = -3.95340937166299; a3 = 5.86341125073283; a4 = -3.86654437944130; a5 = 0.956543676511204;
        b1 = 0.000241359048974918;	b2=0.0;	b3=-0.000482718097949836;	b4=0.0;	b5=0.000241359048974918;
        break;
    case 2:
        a1 = 1;	a2 = -3.91522312056265; a3=5.75949354217813;	a4=-3.77286854920398;	a5=0.928627086124806;
        b1=0.000660779098216284;	b2=0.0;	b3=-0.00132155819643257;	b4=0.0;	b5=0.000660779098216284;
        break;
    case 3:
        a1=1;	a2=-3.92741385494988;	a3=5.81218855122669;	a4=-3.84112004070264;	a5=0.956543676511202;
        b1=0.000241359049046793;	b2=0;	b3=-0.000482718098093586;	b4=0.0;	b5=0.000241359049046793;
        break;
    default:
        qDebug()<<bandNo<<"is not supported";
        }

    for (i =0;i<SAMPLESPERPOINT;i++)
    {
        //input acc for the IIR
           inAcc = in[i];
           inAcc = inAcc - (a2*buf1);
           inAcc = inAcc - (a3*buf2);
           inAcc = inAcc - (a4*buf3);
           inAcc = inAcc - (a5*buf4);

       //output acc for the IIR (FIR part in IIR)
           outAcc = b1*inAcc;
           outAcc = outAcc + (b2*buf1);
           outAcc = outAcc + (b3*buf2);
           outAcc = outAcc + (b4*buf3);
           outAcc = outAcc + (b5*buf4);

           buf4 = buf3;
           buf3 = buf2;
           buf2 = buf1;
           buf1 = inAcc;

           outBuff[i] = round(outAcc);
    }
    /*
        double a[NUMOFBANDS][5];
        double b[NUMOFBANDS][5];

        a[NUMOFBANDS][5]=
               {{1.0,	-3.95340937166299,	5.86341125073283,	-3.86654437944130,	0.956543676511204},
                {1.0,	-3.91522312056265,	5.75949354217813,	-3.77286854920398,	0.928627086124806},
                {1.0,	-3.92741385494988,	5.81218855122669,	-3.84112004070264,	0.956543676511202}};

        b[NUMOFBANDS][5]=
               {{0.000241359048974918,	0,	-0.000482718097949836,	0,	0.000241359048974918},
                {0.000660779098216284,	0,	-0.00132155819643257,	0,	0.000660779098216284},
                {0.000241359049046793,	0,	-0.000482718098093586,	0,	0.000241359049046793}};


    {//filter testing
        QElapsedTimer timer;
        timer.start();
        short in[500] = {0};
        short out[500] = {0};
        in[0] = 1<<14;
        for (int j=0; j<100;j++)
            IIRfilter(in,out,3);
        qDebug()<<in[0]<<out[0]<<timer.elapsed();
    }
    */

}
inline short coreProcessor::optMed9(short * p)
{
    SORT(p[1], p[2]) ; SORT(p[4], p[5]) ; SORT(p[7], p[8]) ;
    SORT(p[0], p[1]) ; SORT(p[3], p[4]) ; SORT(p[6], p[7]) ;
    SORT(p[1], p[2]) ; SORT(p[4], p[5]) ; SORT(p[7], p[8]) ;
    SORT(p[0], p[3]) ; SORT(p[5], p[8]) ; SORT(p[4], p[7]) ;
    SORT(p[3], p[6]) ; SORT(p[1], p[4]) ; SORT(p[2], p[5]) ;
    SORT(p[4], p[7]) ; SORT(p[4], p[2]) ; SORT(p[6], p[4]) ;
    SORT(p[4], p[2]) ; return(p[4]) ;
}

inline short coreProcessor::optMed25(short * p)
{
    SORT(p[0], p[1]) ; SORT(p[3], p[4]) ; SORT(p[2], p[4]) ;
    SORT(p[2], p[3]) ; SORT(p[6], p[7]) ; SORT(p[5], p[7]) ;
    SORT(p[5], p[6]) ; SORT(p[9], p[10]) ; SORT(p[8], p[10]) ;
    SORT(p[8], p[9]) ; SORT(p[12], p[13]) ; SORT(p[11], p[13]) ;
    SORT(p[11], p[12]) ; SORT(p[15], p[16]) ; SORT(p[14], p[16]) ;
    SORT(p[14], p[15]) ; SORT(p[18], p[19]) ; SORT(p[17], p[19]) ;
    SORT(p[17], p[18]) ; SORT(p[21], p[22]) ; SORT(p[20], p[22]) ;
    SORT(p[20], p[21]) ; SORT(p[23], p[24]) ; SORT(p[2], p[5]) ;
    SORT(p[3], p[6]) ; SORT(p[0], p[6]) ; SORT(p[0], p[3]) ;
    SORT(p[4], p[7]) ; SORT(p[1], p[7]) ; SORT(p[1], p[4]) ;
    SORT(p[11], p[14]) ; SORT(p[8], p[14]) ; SORT(p[8], p[11]) ;
    SORT(p[12], p[15]) ; SORT(p[9], p[15]) ; SORT(p[9], p[12]) ;
    SORT(p[13], p[16]) ; SORT(p[10], p[16]) ; SORT(p[10], p[13]) ;
    SORT(p[20], p[23]) ; SORT(p[17], p[23]) ; SORT(p[17], p[20]) ;
    SORT(p[21], p[24]) ; SORT(p[18], p[24]) ; SORT(p[18], p[21]) ;
    SORT(p[19], p[22]) ; SORT(p[8], p[17]) ; SORT(p[9], p[18]) ;
    SORT(p[0], p[18]) ; SORT(p[0], p[9]) ; SORT(p[10], p[19]) ;
    SORT(p[1], p[19]) ; SORT(p[1], p[10]) ; SORT(p[11], p[20]) ;
    SORT(p[2], p[20]) ; SORT(p[2], p[11]) ; SORT(p[12], p[21]) ;
    SORT(p[3], p[21]) ; SORT(p[3], p[12]) ; SORT(p[13], p[22]) ;
    SORT(p[4], p[22]) ; SORT(p[4], p[13]) ; SORT(p[14], p[23]) ;
    SORT(p[5], p[23]) ; SORT(p[5], p[14]) ; SORT(p[15], p[24]) ;
    SORT(p[6], p[24]) ; SORT(p[6], p[15]) ; SORT(p[7], p[16]) ;
    SORT(p[7], p[19]) ; SORT(p[13], p[21]) ; SORT(p[15], p[23]) ;
    SORT(p[7], p[13]) ; SORT(p[7], p[15]) ; SORT(p[1], p[9]) ;
    SORT(p[3], p[11]) ; SORT(p[5], p[17]) ; SORT(p[11], p[17]) ;
    SORT(p[9], p[17]) ; SORT(p[4], p[10]) ; SORT(p[6], p[12]) ;
    SORT(p[7], p[14]) ; SORT(p[4], p[6]) ; SORT(p[4], p[7]) ;
    SORT(p[12], p[14]) ; SORT(p[10], p[14]) ; SORT(p[6], p[7]) ;
    SORT(p[10], p[12]) ; SORT(p[6], p[10]) ; SORT(p[6], p[17]) ;
    SORT(p[12], p[17]) ; SORT(p[7], p[17]) ; SORT(p[7], p[10]) ;
    SORT(p[12], p[18]) ; SORT(p[7], p[12]) ; SORT(p[10], p[18]) ;
    SORT(p[12], p[20]) ; SORT(p[10], p[20]) ; SORT(p[10], p[12]) ;
    return (p[12]);
}

// for 7X7 filter
inline short coreProcessor::optMed49(short *p)
{
    short i,k;
    for (i=1; i<49; i++)
    {
        for (k = i; (k > 0 && p[k] < p[k-1]); k--)
            SWAP(p[k], p[k-1])
    }
    return (p[24]);
 }


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------------//
//------------------------------------------------------------- movie making code from here onwards---------------------------//
//----------------------------------------------------------------------------------------------------------------------------//

// This is the internal structure represented by the HAVI handle:
typedef struct
{
    IAVIFile *pfile;    // created by CreateAvi
    WAVEFORMATEX wfx;   // as given to CreateAvi (.nChanels=0 if none was given). Used when audio stream is first created.
    int period;         // specified in CreateAvi, used when the video stream is first created
    IAVIStream *as;     // audio stream, initialised when audio stream is first created
    IAVIStream *ps, *psCompressed;  // video stream, when first created
    unsigned long nframe, nsamp;    // which frame will be added next, which sample will be added next
    bool iserr;         // if true, then no function will do anything
} TAviUtil;
HAVI CreateAvi(const char *fn, int frameperiod, const WAVEFORMATEX *wfx)
{
    IAVIFile *pfile;
    AVIFileInit();
    WCHAR y[255];
    LPCWSTR py = y;
    ::MultiByteToWideChar(0, 0, fn, -1, y, 255);
    HRESULT hr = AVIFileOpen(&pfile, py, OF_WRITE|OF_CREATE, NULL);
    if (hr != AVIERR_OK) {
        AVIFileExit();
        return NULL;
    }
    TAviUtil *au = new TAviUtil;
    au->pfile = pfile;
    if (wfx == NULL)
        ZeroMemory(&au->wfx, sizeof(WAVEFORMATEX));
    else
        CopyMemory(&au->wfx, wfx, sizeof(WAVEFORMATEX));
    au->period = frameperiod;
    au->as = 0;
    au->ps = 0;
    au->psCompressed = 0;
    au->nframe = 0;
    au->nsamp = 0;
    au->iserr = false;
    return (HAVI)au;
}
HRESULT CloseAvi(HAVI avi)
{
    if (avi == NULL)
        return AVIERR_BADHANDLE;
    TAviUtil *au = (TAviUtil*)avi;
    if (au->as != 0)
        AVIStreamRelease(au->as);
    au->as=0;
    if (au->psCompressed != 0)
        AVIStreamRelease(au->psCompressed);
    au->psCompressed = 0;
    if (au->ps != 0)
        AVIStreamRelease(au->ps);
    au->ps=0;
    if (au->pfile != 0)
        AVIFileRelease(au->pfile);
    au->pfile = 0;
    AVIFileExit();
    delete au;
    return S_OK;
}

HRESULT AddAviFrame(HAVI avi, HBITMAP hbm)
{
    if (avi == NULL)
        return AVIERR_BADHANDLE;
    if (hbm == NULL)
        return AVIERR_BADPARAM;
    DIBSECTION dibs;
    int sbm = GetObject(hbm, sizeof(dibs), &dibs);
    if (sbm != sizeof(DIBSECTION))
        return AVIERR_BADPARAM;
    TAviUtil *au = (TAviUtil*)avi;
    if (au->iserr)
        return AVIERR_ERROR;
    // create the stream, if it wasn't there before
    if (au->ps == 0) {
        AVISTREAMINFO strhdr;
        ZeroMemory(&strhdr,sizeof(strhdr));
        strhdr.fccType = streamtypeVIDEO;// stream type
        strhdr.fccHandler = 0;
        strhdr.dwScale = au->period;
        strhdr.dwRate = 1000;
        strhdr.dwSuggestedBufferSize  = dibs.dsBmih.biSizeImage;
        SetRect(&strhdr.rcFrame, 0, 0, dibs.dsBmih.biWidth, dibs.dsBmih.biHeight);
        HRESULT hr = AVIFileCreateStream(au->pfile, &au->ps, &strhdr);
        if (hr != AVIERR_OK) {
            au->iserr = true;
            return hr;
        }
    }
    //
    // create an empty compression, if the user hasn't set any
    if (au->psCompressed == 0) {
        AVICOMPRESSOPTIONS opts;
        ZeroMemory(&opts, sizeof(opts));
        //opts.
        opts.fccHandler = mmioFOURCC('D','I','B',' ');
        HRESULT hr = AVIMakeCompressedStream(&au->psCompressed, au->ps, &opts, NULL);
        if (hr != AVIERR_OK) {
            au->iserr = true;
            return hr;
        }
        hr = AVIStreamSetFormat(au->psCompressed, 0, &dibs.dsBmih, dibs.dsBmih.biSize+dibs.dsBmih.biClrUsed*sizeof(RGBQUAD));
        if (hr != AVIERR_OK) {
            au->iserr = true;
            return hr;
        }
    }
    //
    //Now we can add the frame
    HRESULT hr = AVIStreamWrite(au->psCompressed, au->nframe, 1, dibs.dsBm.bmBits, dibs.dsBmih.biSizeImage, AVIIF_KEYFRAME, NULL, NULL);
    if (hr != AVIERR_OK) {
        au->iserr = true;
        return hr;
    }
    au->nframe++; return S_OK;
}
