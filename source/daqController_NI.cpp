#include "daqController.h"

daqController::daqController(structDaq *daqInfoPtrArg, structScan *scanInfoPtrArg)
{
    daqInfoPtr      = daqInfoPtrArg;
    scanInfoPtr     = scanInfoPtrArg;
    ConfigDone      = false;
    binaryWfm       = NULL;
    filteredWfm     = NULL;
    OsciModeEn      = true;
    fetcherObj      = NULL;
    allowOsciUpdate = false;
#if ACTUALSYSTEM
    this->signalChannel = "0";
#endif
    getHandle();

    updateTimerdaq = new QTimer(this);
    connect(updateTimerdaq, SIGNAL(timeout()), this, SLOT(setallowOsciUpdate()));
}

daqController::~daqController()
{
    //Close the current session
    // Close the session
#if ACTUALSYSTEM
    if (vi)
       niScope_close(vi);
#endif
    if (binaryWfm != NULL)
        delete binaryWfm;
    if (filteredWfm != NULL)
        delete filteredWfm;
}

void daqController::getHandle()
{
#if ACTUALSYSTEM
    ViStatus error = VI_SUCCESS;
    ViChar   errorMessage[MAX_ERROR_DESCRIPTION] = " ";
    ViChar   errorSource[MAX_FUNCTION_NAME_SIZE];
    //Signal Settings
    ViChar resourceName[10] = "Dev1";
    //ViRsrc resourceName = "Dev1";

    //Caution  : Resetting the digitizer may cause wear on the relays, so you should reset only when necessary.
    //binaryWfmPtr = binaryWfm.data();
    // Open the NI-SCOPE instrument handle
    handleErr (niScope_init(resourceName, VI_TRUE , NISCOPE_VAL_FALSE, &vi));
    Error:
    if (error != VI_SUCCESS)
    {
       niScope_errorHandler (vi, error, errorSource, errorMessage);   // Intrepret the error
       QString errorString = errorSource ;
       errorString.append(errorMessage);
       errorHandler(errorString);
    }
    else
    {
       qDebug()<<"Successfully opened a session";
    }
#endif
}

void daqController::Configure( bool OsciModeEnArg)
{
#if ACTUALSYSTEM
    ViStatus error = VI_SUCCESS;
    ViChar   errorMessage[MAX_ERROR_DESCRIPTION] = " ";
    ViChar   errorSource[MAX_FUNCTION_NAME_SIZE];

    //Signal Settings
    //ViChar resourceName[MAX_STRING_SIZE] = "Dev1";
    //ViInt32  triggerType    = 0; //Edge
    ViReal64 verticalRange  = 1.5; //-> 1.0 is correctly applied, 1.5 is mapped to 3.49
    ViReal64 verticalOffset = 0.0;
    ViReal64 minSampleRate  = (ViReal64)daqInfoPtr->SamplingFreq * 1000000;
    //ViInt32 BytesPerShort = 2;
    ViInt32 numWaveform;
    ViInt32 measWfmLength;

    //Trigger settings
    ViReal64 trigVerticalRange  = 2.0;
    ViReal64 trigVerticalOffset = 0.0;
    ViInt32  triggerCoupling = NISCOPE_VAL_DC;
    ViInt32  triggerSlope = NISCOPE_VAL_POSITIVE;
    ViReal64 triggerLevel = 1.35;
    ViReal64 triggerHoldoff = 0.0;
    ViReal64 triggerDelay = (daqInfoPtr->SamplingFreq==10) ? 0.000020 : 0.000009;
    //ViReal64 triggerDelay = 0; //(daqInfoPtr->SamplingFreq==10) ? 0.000020 : 0.000008;
    //ViReal64 triggerDelay = 0.001;
    ViConstString triggerChannel = "2";//(ViConstString)daqInfoPtr->TrigCh.data();
    ViReal64 refPosition = 0.0;
    //ViReal64 timeout = -1; // seconds

    //debug
    /*
    {

//        double filteredWfmTest[1210];
//         //filteredWfm = new double[daqInfoPtr->ScanPoints]();
//         for (int i=0 ; i <daqInfoPtr->ScanPoints;i++)
//         {
//             filteredWfmTest[i]=i%SAMPLESPERPOINT+i/SAMPLESPERPOINT;
//         }

         filteredWfm = new double[daqInfoPtr->ScanPoints*SAMPLESPERPOINT]();
         emit scanFinished(filteredWfm);
    }
*/
    if (vi){}
        else
            getHandle();
    this->ConfigDone = false; //passimism.

    if(this->fetcherObj!=NULL)
    {
        StopAcquisition();
        while(this->fetcherObj!=NULL)//wait to for the exit of prev thread
            QCoreApplication::processEvents();
    }

    // Configure the vertical parameters
    //ViStatus niScope_ConfigureVertical (ViSession vi, ViConstString channelList, ViReal64 range,ViReal64 offset,
    //                                    ViInt32 coupling, ViReal64 probeAttenuation, ViBoolean enabled);

    handleErr (niScope_ConfigureVertical (vi, signalChannel, verticalRange, verticalOffset,NISCOPE_VAL_AC, 1.0, NISCOPE_VAL_TRUE));

    handleErr (niScope_ConfigureVertical (vi, triggerChannel, trigVerticalRange, trigVerticalOffset,NISCOPE_VAL_DC, 1.0, NISCOPE_VAL_TRUE));

    // Configure the horizontal parameters
    //ViStatus niScope_ConfigureHorizontalTiming (ViSession vi, ViReal64 minSampleRate, ViInt32 minNumPts,
    //                                             ViReal64 refPosition, ViInt32 numRecords, ViBoolean enforceRealtime);

    // Set the attribute to allow more records than memory

    if (OsciModeEnArg)
    {
        checkErr (niScope_SetAttributeViInt32 ( vi, VI_NULL, NISCOPE_ATTR_FETCH_NUM_RECORDS, -1));
        checkErr (niScope_SetAttributeViInt32 ( vi, VI_NULL, NISCOPE_ATTR_FETCH_RECORD_NUMBER, 0));
        handleErr (niScope_SetAttributeViBoolean(vi, VI_NULL, NISCOPE_ATTR_ALLOW_MORE_RECORDS_THAN_MEMORY, VI_FALSE));
        handleErr (niScope_ConfigureHorizontalTiming (vi, minSampleRate, SAMPLESPERPOINT, refPosition,1, VI_TRUE));
        updateTimerdaq->stop();
    }
    else
    {
        handleErr (niScope_SetAttributeViBoolean(vi, VI_NULL, NISCOPE_ATTR_ALLOW_MORE_RECORDS_THAN_MEMORY, VI_TRUE));
        handleErr (niScope_ConfigureHorizontalTiming (vi, minSampleRate, SAMPLESPERPOINT, refPosition,daqInfoPtr->ScanPoints, VI_TRUE));
        updateTimerdaq->start(100);
    }

    // Configure the trigger -- edge
    //ViStatus niScope_ConfigureTriggerEdge (ViSession vi, ViConstString triggerSource, ViReal64 level,
    //                                       ViInt32 slope, ViInt32 triggerCoupling,
    //                                       ViReal64 holdoff, ViReal64 delay);

    handleErr (niScope_ConfigureTriggerEdge (vi, triggerChannel, triggerLevel,triggerSlope, triggerCoupling,triggerHoldoff, triggerDelay));


    //ViStatus niScope_ConfigureChanCharacteristics (ViSession vi, ViConstString channelList,
    //                                               ViReal64 inputImpedance, ViReal64 maxInputFrequency);

    //handleErr (niScope_ConfigureChanCharacteristics(vi,signalChannel,NISCOPE_VAL_50_OHMS,-1));
    //handleErr (niScope_ConfigureChanCharacteristics(vi,triggerChannel,NISCOPE_VAL_50_OHMS,-1))

    handleErr (niScope_ConfigureChanCharacteristics(vi,signalChannel,NISCOPE_VAL_1_MEG_OHM,-1));
    handleErr (niScope_ConfigureChanCharacteristics(vi,triggerChannel,NISCOPE_VAL_1_MEG_OHM,-1))


    // Find out the number of waveforms, based on # of channels and records
    handleErr (niScope_ActualNumWfms (vi, signalChannel, &numWaveform));

    // Query the coerced record length
    handleErr (niScope_ActualRecordLength (vi, &actualRecordLength));

    // Allocate space for the waveform ,waveform info, and binary waveform according to the record length and number of waveforms
    wfmInfoPtr = (niScope_wfmInfo *) malloc (sizeof (struct niScope_wfmInfo) * numWaveform);

    //binaryWfm = new short[actualRecordLength * numWaveform](); //using() for memory init to 0
    if (actualRecordLength != SAMPLESPERPOINT)
    {
        errorHandler("daqController::Configure() : actualRecordLength is not equal to SAMPLESPERPOINT:"+QString::number(SAMPLESPERPOINT));
        return;
    }

    if(daqInfoPtr->ButterWorthIIREnabled)
    {
        /*
        ViReal64 startFreq,stopFreq,bandWidth,cntrFreq; // all in KHz
        if (daqInfoPtr->freqMode == 0) //LowFreq
        {
            startFreq   = 50;
            stopFreq    =

;

            bandWidth = 150*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
            cntrFreq  = 125*1000;//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);

            //bandWidth = 240*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
            //cntrFreq  = 130*1000;//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);
        }
        else if (daqInfoPtr->freqMode == 1) //MidFreq
        {
            startFreq   = 200;
            stopFreq    = 1300;

            //bandWidth = 1100*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
            //cntrFreq  = 750*1000;//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);

            bandWidth = (stopFreq-startFreq)*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
            cntrFreq  = (startFreq*1000)+(bandWidth/2);//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);

            //bandWidth = 240*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
            //cntrFreq  = 130*1000;//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);
        }
        else if (daqInfoPtr->freqMode == 2) //HiFreq
        {
            startFreq   = 1300;
            stopFreq    = 3000;

            bandWidth = (stopFreq-startFreq)*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
            cntrFreq  = (startFreq*1000)+(bandWidth/2);//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);

            //bandWidth = 240*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
            //cntrFreq  = 130*1000;//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);
        }
        */
        ViReal64 startFreq,stopFreq; //KHz
        ViReal64 bandWidth,cntrFreq;

        startFreq   = daqInfoPtr->startFreqBandPass;
        stopFreq    = daqInfoPtr->stopFreqBandPass;
        bandWidth   = (stopFreq-startFreq)*1000;//(daqInfoPtr->bandStopKhz - daqInfoPtr->bandStartKhz)*1000;
        cntrFreq    = (startFreq*1000)+(bandWidth/2);//(daqInfoPtr->bandStartKhz * 1000) + (bandWidth/2);

        qDebug()<<"daqController::Configure"<<"daqInfoPtr->freqMode"<<daqInfoPtr->freqMode<<
                  "startFreq"   <<startFreq<<
                  "stopFreq"    <<stopFreq<<
                  "bandWidth"   <<bandWidth<<
                  "cntrFreq"    <<cntrFreq;


        //ViStatus niScope_SetAttributeViInt32 (ViSession vi, ViConstString channelList, ViAttr attributeID, ViInt32 value);
        handleErr(niScope_SetAttributeViInt32 (vi,signalChannel, NISCOPE_ATTR_MEAS_FILTER_TYPE, NISCOPE_VAL_MEAS_BANDPASS ));
        handleErr(niScope_SetAttributeViInt32 (vi,signalChannel, NISCOPE_ATTR_MEAS_FILTER_ORDER, 1 )); // order was not set in example
        handleErr(niScope_SetAttributeViReal64 (vi,signalChannel, NISCOPE_ATTR_MEAS_FILTER_CENTER_FREQ, cntrFreq )); // centre freq in Hz
        handleErr(niScope_SetAttributeViReal64 (vi,signalChannel, NISCOPE_ATTR_MEAS_FILTER_WIDTH, bandWidth )); // cutoff = centre ± one-half width
        handleErr(niScope_SetAttributeViReal64 (vi,signalChannel, NISCOPE_ATTR_MEAS_FILTER_TRANSIENT_WAVEFORM_PERCENT, 0.0 ));

         // Add a measurement step to the waveform
        //handleErr (niScope_AddWaveformProcessing(vi, signalChannel, NISCOPE_VAL_BUTTERWORTH_FILTER)); //2 filters
        handleErr (niScope_ActualMeasWfmSize (vi, NISCOPE_VAL_BUTTERWORTH_FILTER, &measWfmLength));
        //filteredWfm = new double[actualRecordLength * numWaveform](); //measWfmLength is not correct for some reason
        if (filteredWfm!=NULL)
        {
            delete filteredWfm;
        }
        filteredWfm = new double[SIG_FIFO_LEN * actualRecordLength](); //actual recored length should be eq to SAMPLESPERPOINT

    }
    else
    {
        if (binaryWfm!=NULL)
        {
            delete binaryWfm;
        }
        binaryWfm = new short[SIG_FIFO_LEN * actualRecordLength](); //using() for memory init to 0
    }

    handleErr(niScope_InitiateAcquisition (vi));

    Error:
    if (error != VI_SUCCESS)
    {
       niScope_errorHandler (vi, error, errorSource, errorMessage);   // Intrepret the error
       QString errorString = errorSource ;
       errorString.append(errorMessage);
       errorHandler(errorString);
    }
    else
    {
       qDebug()<<"daqController::Configure - Successfully configured the Daq card."<<"measWfmLength (not used)"<<measWfmLength<<"actualRecordLength"<<actualRecordLength;
       ConfigDone = true;
       this->OsciModeEn = OsciModeEnArg;
       this->StartAcquisition();
    }
#endif
}

void daqController::StartAcquisition()
{
    if (ConfigDone)
    {
        //this->curWfmNum = 0;
        configFetchThread();
        fetcherThread->start();
        //fetcherThread->setPriority(QThread::TimeCriticalPriority);
    }
    else
    {
        qDebug()<<"ERROR: Conguration must be done before starting.";
    }
}

void daqController::configFetchThread()
{
  double *WfmPtr;
  fetcherObj = new fetcher;
  fetcherThread = new QThread;
  fetcherObj->moveToThread(fetcherThread);

  connect(fetcherObj, SIGNAL(errorSignal(QString)), this, SLOT(errorHandler(QString)));
  connect(fetcherObj, SIGNAL(newArrival(qint32)), this, SLOT(newArrivalInDaq(qint32)));
  connect(fetcherObj, SIGNAL(finished(int)), this, SLOT(acqFinished(int)));
  connect(fetcherObj, SIGNAL(finished(int)), fetcherThread, SLOT(quit()));
  connect(fetcherObj, SIGNAL(finished(int)), fetcherObj, SLOT(deleteLater()));
  connect(fetcherThread, SIGNAL(finished()), fetcherThread, SLOT(deleteLater()));

  if (this->OsciModeEn==false)
     connect(fetcherThread, SIGNAL(started()), fetcherObj, SLOT(fetch()));
  else
     connect(fetcherThread, SIGNAL(started()), fetcherObj, SLOT(fetchOsciMode()));

  // Waveforms
  if(daqInfoPtr->ButterWorthIIREnabled)
      WfmPtr = filteredWfm;
  //else
      //WfmPtr = binaryWfm;
#if ACTUALSYSTEM
  fetcherObj->setPars(vi,signalChannel,actualRecordLength, WfmPtr, wfmInfoPtr, daqInfoPtr->ScanPoints,
                      daqInfoPtr->ButterWorthIIREnabled,(scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1,OsciModeEn);
#endif

}

void daqController::StopAcquisition()
{
    if (fetcherObj!=NULL)
        fetcherObj->reset();
}

void daqController::acqFinished(int lastWaveFormNum)
{
#if ACTUALSYSTEM
    niScope_Abort(vi);
    this->fetcherObj = NULL;
    if (OsciModeEn == false)
    {
        emit updateProgressBar_daqControllerSignal((double)(100));
        if (lastWaveFormNum!=daqInfoPtr->ScanPoints)
        {
            QString StatusTip = "WARNING!!! Unexpected finish of data acquistion - "+(QString::number(lastWaveFormNum) + "/" + QString::number(daqInfoPtr->ScanPoints));
            emit updateStatusBar_daqControllerSignal(StatusTip);
        }
        emit scanFinished(lastWaveFormNum); //always butterworth should be enabled
        //if(ButterWorthIIREnabled_f)
            //handleErr (niScope_ClearWaveformProcessing(vi_f, signalChannel_f));
    }
    else
    { // do nothing

    }
#endif
}

void daqController::errorHandler(QString ErrorMessage)
{
    QString Delimitor = "-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*";

    qDebug()<<"";
    qDebug()<<Delimitor;
    qDebug()<<ErrorMessage;

    QMessageBox msgBox(QMessageBox::Critical, tr("DAQ Error"),ErrorMessage,QMessageBox::NoButton, 0);
    msgBox.exec();
}

void daqController::newArrivalInDaq(qint32 waveformNumber )
{
    if (OsciModeEn == false)
    {
        //generate signal to dataProcessor to copy the data
        emit newWfmReadyForCopy(filteredWfm,waveformNumber);
        if (allowOsciUpdate)
        {
            emit updatePlotOsci((double*) &filteredWfm[(waveformNumber*SAMPLESPERPOINT)%(SAMPLESPERPOINT*SIG_FIFO_LEN)]);
            allowOsciUpdate = false;
        }
        emit updateProgressBar_daqControllerSignal(((double)(waveformNumber+1)/daqInfoPtr->ScanPoints)*100);
        QString StatusTip = (QString::number(waveformNumber+1) + "/" + QString::number(daqInfoPtr->ScanPoints));
        emit updateStatusBar_daqControllerSignal(StatusTip);
    }
    else
    { //osci mode generate signal directly to plot
        emit updatePlotOsci(filteredWfm);
    }
}

void daqController::setallowOsciUpdate()
{
    this->allowOsciUpdate = true;
}

fetcher::fetcher()
{
    this->reqWfmCnt_f = 0;
    this->fetchedWfmCnt_f = 0;
    this->OsciModeEn_f = false;
}

fetcher::~fetcher()
{

}
/*
void fetcher::resetOsciMode()
{
    this->OsciModeEn_f = false;

}
*/
void fetcher::reset()
{
    this->reqWfmCnt_f = fetchedWfmCnt_f;
    this->OsciModeEn_f = false;
    //this->fetchedWfmCnt_f = 0;
}
#if ACTUALSYSTEM
void fetcher::setPars(ViSession vi_arg, ViConstString signalChannel_arg, ViInt32 actualRecordLength_arg,
                      double  *WfmPtr_arg, struct niScope_wfmInfo *wfmInfoPtr_arg,qint32 reqWfmCnt_arg,
                      bool ButterWorthIIREnabled_arg,short x_length_arg, bool OsciModeEn_arg)
{
    vi_f                    = vi_arg;
    signalChannel_f         = signalChannel_arg;
    actualRecordLength_f    = actualRecordLength_arg;
    WfmPtr_f                = WfmPtr_arg;
    wfmInfoPtr_f            = wfmInfoPtr_arg;
    reqWfmCnt_f             = reqWfmCnt_arg;
    ButterWorthIIREnabled_f = ButterWorthIIREnabled_arg;
    fetchedWfmCnt_f         = 0;
    x_length_f              = x_length_arg;
    FirstTime               = true; // just for debug
    OsciModeEn_f            = OsciModeEn_arg;
}
#endif

void fetcher::fetchOsciMode()
{
#if ACTUALSYSTEM
    ViStatus error = VI_SUCCESS;
    ViChar   errorMessage[MAX_ERROR_DESCRIPTION] = " ";
    ViChar   errorSource[MAX_FUNCTION_NAME_SIZE];
    ViInt32 recordsAcquired=0,recordsAcquiredPrev=0;
    int inputBufStartIndex = 0;

    while(OsciModeEn_f)
    {
        if(ButterWorthIIREnabled_f)
        {
          //ViStatus niScope_FetchArrayMeasurement (ViSession vi, ViConstString channelList, ViReal64 timeout, ViInt32 arrayMeasFunction,
          //                                        ViInt32 measWfmSize, ViReal64* measWfm, struct niScope_wfmInfo* wfmInfo);
            {
                if (FirstTime)
                {
                    qDebug()<<"fetcher::fetchOsciMode - ******************************************ready and listening";
                    FirstTime = false;
                }
            }
            // Find out how many records have been acquired

            niScope_FetchArrayMeasurement(vi_f, signalChannel_f, 0 ,NISCOPE_VAL_BUTTERWORTH_FILTER, actualRecordLength_f, (ViReal64*) WfmPtr_f, wfmInfoPtr_f);
            handleErr(niScope_InitiateAcquisition (vi_f));
            QThread::msleep(50);
        }
        else
        {
            // Use the 16 bit fetching function
            //always use buterworth just with different passband
            //DO NOT USE FOR NOW SINCE WFM POINTER IS DOUBLE
            //handleErr (niScope_FetchBinary16 (vi_f, signalChannel_f, 10, actualRecordLength_f,(ViInt16*) &WfmPtr_f[fetchedWfmCnt_f*SAMPLESPERPOINT], wfmInfoPtr_f));
        }

        Error:
        if (error != VI_SUCCESS && error !=-1074118647 )//-1074118647 ->fetch timeOut
        {
           niScope_errorHandler (vi_f, error, errorSource, errorMessage);   // Intrepret the error
           QString errorString = errorSource ;
           errorString.append(errorMessage);
           emit errorSignal(errorString);
           break;
        }
        else
        {
            emit newArrival(0);
        }
    }
    emit finished(0);
#endif
}

void fetcher::fetch()
{
#if ACTUALSYSTEM
    ViStatus error = VI_SUCCESS;
    ViChar   errorMessage[MAX_ERROR_DESCRIPTION] = " ";
    ViChar   errorSource[MAX_FUNCTION_NAME_SIZE];
    ViInt32 recordsAcquired=0,recordsAcquiredPrev=0;
    int inputBufStartIndex = 0;
    int waitForSigTh = 5;
    int waitForSigCnt = 0;

    while((fetchedWfmCnt_f < reqWfmCnt_f) && (reqWfmCnt_f > 0))
    {
        //handleErr (niScope_InitiateAcquisition (vi_f));
        if(ButterWorthIIREnabled_f)
        {
          //ViStatus niScope_FetchArrayMeasurement (ViSession vi, ViConstString channelList, ViReal64 timeout, ViInt32 arrayMeasFunction,
          //                                        ViInt32 measWfmSize, ViReal64* measWfm, struct niScope_wfmInfo* wfmInfo);
            {
                if (FirstTime)
                {
                    qDebug()<<"fetcher::fetch - ******************************************ready and listening";
                    FirstTime = false;
                }
            }

            // Find out how many records have been acquired
            do
            {
                #ifdef DAQ_DEBUG_LOGS
                qDebug()<<QDateTime::currentDateTime().time()<<"waitForSigTh"<<waitForSigTh<<"waitForSigCnt"<<waitForSigCnt<<"recordsAcquired"<<recordsAcquired<<
                          "recordsAcquiredPrev"<<recordsAcquiredPrev<<"inputBufStartIndex"<<inputBufStartIndex;
                #endif
                checkErr (niScope_GetAttributeViInt32 ( vi_f, VI_NULL, NISCOPE_ATTR_RECORDS_DONE, &recordsAcquired));
                checkErr (niScope_SetAttributeViInt32 ( vi_f, VI_NULL, NISCOPE_ATTR_FETCH_NUM_RECORDS, 1));
                checkErr (niScope_SetAttributeViInt32 ( vi_f, VI_NULL, NISCOPE_ATTR_FETCH_RECORD_NUMBER, fetchedWfmCnt_f));
                //handleErr(niScope_Fetch (vi_f, signalChannel_f, 10, actualRecordLength_f, (ViReal64*) &WfmPtr_f[fetchedWfmCnt_f*SAMPLESPERPOINT], wfmInfoPtr_f));
                inputBufStartIndex = (fetchedWfmCnt_f*SAMPLESPERPOINT)%(SAMPLESPERPOINT*SIG_FIFO_LEN);
                //niScope_FetchArrayMeasurement(vi_f, signalChannel_f, 2,NISCOPE_VAL_BUTTERWORTH_FILTER, actualRecordLength_f, (ViReal64*) &WfmPtr_f[inputBufStartIndex], wfmInfoPtr_f);
                niScope_FetchArrayMeasurement(vi_f, signalChannel_f, 2,NISCOPE_VAL_BUTTERWORTH_FILTER, actualRecordLength_f, (ViReal64*) &WfmPtr_f[inputBufStartIndex], wfmInfoPtr_f);
                waitForSigCnt++;


                if(waitForSigCnt == waitForSigTh )
                    break;
            }
            while (recordsAcquired==recordsAcquiredPrev);
            waitForSigCnt=0;
            recordsAcquiredPrev = recordsAcquired;

           // handleErr (niScope_FetchArrayMeasurement (vi_f, signalChannel_f, 10, NISCOPE_VAL_BUTTERWORTH_FILTER ,actualRecordLength_f, (ViReal64*) &WfmPtr_f[fetchedWfmCnt_f*SAMPLESPERPOINT], wfmInfoPtr_f));
          /*
          {//debug
              double dbgBuf[500];
              double *dbgPtr = &WfmPtr_f[fetchedWfmCnt_f*SAMPLESPERPOINT];
              for (int i =0;i<500;i++)
              {
                      dbgBuf[i] =dbgPtr[i];
              }
            qDebug()<<"DebugTime"<<fetchedWfmCnt_f<<dbgPtr<<dbgBuf[100]<<dbgBuf[101]<<dbgBuf[102]<<dbgBuf[103]<<dbgBuf[104];
          }
          */
        }
        else
        {
            // Use the 16 bit fetching function
            //always use buterworth just with different passband
            //DO NOT USE FOR NOW SINCE WFM POINTER IS DOUBLE
            //handleErr (niScope_FetchBinary16 (vi_f, signalChannel_f, 10, actualRecordLength_f,(ViInt16*) &WfmPtr_f[fetchedWfmCnt_f*SAMPLESPERPOINT], wfmInfoPtr_f));
        }

        Error:
        if (error != VI_SUCCESS)
        {
           niScope_errorHandler (vi_f, error, errorSource, errorMessage);   // Intrepret the error
           QString errorString = errorSource ;
           errorString.append(errorMessage);
           emit errorSignal(errorString);
           break;
        }
        else
        {
          #ifdef DAQ_DEBUG_LOGS
           //voltage = binary data × gain factor + offset
           //qDebug()<<wfmInfoPtr_f->relativeInitialX<<"\t"<<wfmInfoPtr_f->xIncrement<<"\t"<<wfmInfoPtr_f->actualSamples<<"\t"<<wfmInfoPtr_f->gain<<"\t"<<wfmInfoPtr_f->offset;
            qDebug()<<QDateTime::currentDateTime().time()<<"fetchedWfmCnt_f"<<fetchedWfmCnt_f<<"recordsAcquired"<<recordsAcquired<<"inputBufStartIndex"<<inputBufStartIndex;
          #endif


            if (fetchedWfmCnt_f < recordsAcquired)
            {
                emit newArrival( fetchedWfmCnt_f );
                fetchedWfmCnt_f++;
                //we should not wait for long before aborting once the scan has started
                //consider to change the value based on scan speed
                waitForSigTh = 3;
            }
            else
            {
                //the expected waveform number has gone ahead of the recordsAcquired so far... leave!!!
                niScope_Abort(vi_f);
                break;
            }
        }
    }
    emit finished(fetchedWfmCnt_f);
#endif
}
