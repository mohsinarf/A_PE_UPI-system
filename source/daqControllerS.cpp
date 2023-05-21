#include "daqControllerS.h"

daqController::daqController(structDaq *daqInfoPtrArg, structScan *scanInfoPtrArg)
{
    daqInfoPtr      = daqInfoPtrArg;
    scanInfoPtr     = scanInfoPtrArg;
    ConfigDone      = false;
    binaryWfmBuff       = NULL;
    //filteredWfm     = NULL;
    OsciModeEn      = true;
    fetcherObj      = NULL;
    allowOsciUpdate = false;
    pstCard = new ST_SPCM_CARDINFO;

    getHandle();

    updateTimerdaq = new QTimer(this);
    connect(updateTimerdaq, SIGNAL(timeout()), this, SLOT(setallowOsciUpdate()));
}

daqController::~daqController()
{
    // clean up and close the driver
    if(this->fetcherObj!=NULL) // incase previous thread is running, exit it before closing
    {
        StopAcquisition();
        //while(this->fetcherObj!=NULL);//wait to for the exit of prev thread
        //    QCoreApplication::processEvents();
    }

    vSpcMCloseCard (pstCard);
    delete pstCard;

    //free memory
    if (binaryWfmBuff && !bContMemUsed)
    {
           vFreeMemPageAligned (binaryWfmBuff,dwDataBufLen );
           binaryWfmBuff = 0;
    }
}

void daqController::getHandle()
{
    char szBuffer[1024];     // a character buffer for any messages
    uint32 dwError    =   ERR_OK;
    bool success = false;

    // init card number 0 (the first card in the system), get some information and print it
    if (success = bSpcMInitCardByIdx (pstCard, 0))
    {
        dwError = spcm_dwGetParam_i32(pstCard->hDrv, SPC_MIINST_ISDEMOCARD, &isDemoCard);
        if (isDemoCard && !dwError)
        {
/*
#define     SPCM_DEMOWAVEFORM_SINE      0x00000001
#define     SPCM_DEMOWAVEFORM_RECT      0x00000002
#define     SPCM_DEMOWAVEFORM_TRIANGLE  0x00000004
*/
            spcm_dwSetParam_i32(pstCard->hDrv, SPC_DEMOWAVEFORM, SPCM_DEMOWAVEFORM_SINE );
        }
        qDebug()<<(pszSpcMPrintCardInfo (pstCard, szBuffer, sizeof (szBuffer)));
    }
    else
        qDebug()<<"Error: Could not open card";
        //qDebug()<<nSpcMErrorMessageStdOut (pstCard, "Error: Could not open card\n", true);


    //dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_M2CMD, M2CMD_CARD_RESET);
    if (!success)
        daqErrorHandler(dwError, pstCard);
}

void daqController::Configure( bool OsciModeEnArg)
{
    uint32 dwError = ERR_OK;
    bool success = false;
    uint64 qwContBufLen = 0;

    if(this->fetcherObj!=NULL) // incase previous thread is running, exit it before transitioning
    {
        StopAcquisition();
        while(this->fetcherObj!=NULL)//wait to for the exit of prev thread
            QCoreApplication::processEvents();
    }

    //thou shall not move
        this->OsciModeEn = OsciModeEnArg;
    // ------------------------------------------------------------------------
    // do the card setup, error is routed in the structure so we don't care for the return values

    if (this->OsciModeEn == false)
    {
        if (scanInfoPtr->PRF.toFloat()< 1)
            burstSize =512;
        else if (scanInfoPtr->PRF.toFloat()>= 1 && scanInfoPtr->PRF.toFloat()<= 2)
            burstSize =512;
        else if (scanInfoPtr->PRF.toFloat()> 2 && scanInfoPtr->PRF.toFloat()<= 5)
            burstSize =2048;
        else if (scanInfoPtr->PRF.toFloat()> 5 && scanInfoPtr->PRF.toFloat()<= 10)
            burstSize =4096;
        else if (scanInfoPtr->PRF.toFloat()> 10 && scanInfoPtr->PRF.toFloat()<= 20)
            burstSize =8192;
    }
    else
    {
        burstSize = 64;
    }

    burstAlignedScanPoints = daqInfoPtr->ScanPoints;

    if (!pstCard->bSetError)
    {
        // FIFO mode setup, we run continuously and use 128 samples of pretrigger for each segment
        //pstWorkData->lSegmentsize = KILO_B(1);          // segment size
        //pstWorkData->eFileType =    eFT_noWrite;        // storage mode

        // we try to set the samplerate to 1 MHz (M2i) or 20 MHz (M3i, M4i) on internal PLL, no clock output
        // increase this to test the read-out-after-overrun

        //dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_M2CMD, M2CMD_CARD_RESET);
        //**SET SAMPLE RATE
        success = bSpcMSetupClockPLL (pstCard, MEGA(daqInfoPtr->SamplingFreq), false);

        //****************************************
        //**SET TRIGGER EXT
        // we set external trigger for multiple recording
        //success = bSpcMSetupTrigExternal (pstCard, SPC_TM_POS, false); //false sets the trigger to 1M-OHM impedence

        /*
        // external analog trigger with programmable levels
        bool bSpcMSetupTrigExternalLevel (              // returns false if error occured, otherwise true
            ST_SPCM_CARDINFO   *pstCardInfo,            // pointer to a filled card info structure
            int32               lExtMode,               // external trigger mode
            int32               lLevel0 = 1500,         // trigger level 0 (mV)
            int32               lLevel1 = 800,          // trigger level 1 (mV)
            bool                bTrigTerm = true,       // trigger termination active
            bool                bACCoupling = false,    // programmable AC coupling
            int32               lPulsewidth = 0,        // programmable pulsewidth for all external + pulsewidth modes
            bool                bSingleSrc = true,      // acts as single trigger source, all other masks cleared
            int32               lExtLine = 0);          // standard external trigger is line 0
        */
        if (success)
        {
            //success = bSpcMSetupTrigExternalLevel(pstCard,SPC_TM_POS|SPC_TM_REARM,4700,2000,false,true);AOC
            //success = bSpcMSetupTrigExternalLevel(pstCard,SPC_TM_POS|SPC_TM_REARM,2000,1500,false,true); //BRIGHT SOLUTION 2700
			success = bSpcMSetupTrigExternal (pstCard, SPC_TM_POS, false); //false sets the trigger to 1M-OHM impedence
            //trigger delay should be a multiple of 8 samples

            /*
            if(daqInfoPtr->freqMode == 0)
                dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 208);
            else if(daqInfoPtr->freqMode == 1)
                dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 576);
            else if(daqInfoPtr->freqMode == 2)
                dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 32);


            if(daqInfoPtr->freqMode == 0)
            {
                if(scanInfoPtr->enableTT)
                    dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 128);//208 176 ( at 10mm/s/V: 128)
                else
                    dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 128);//208 208 ( at 10mm/s/V: 128)
            }
            else if(daqInfoPtr->freqMode == 1)
            {
                if (daqInfoPtr->Range == 20)
                    dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 256);//576 //640 //504(50mm/s/V), 248(200mm/s/V)
                else if (daqInfoPtr->Range == 50)
                    dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 504);//504 576
            }
            else if(daqInfoPtr->freqMode == 2)
                dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, 32);
*/
            dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_TRIG_DELAY, daqInfoPtr->daqTrigDelay);

        }

        //******************************************************
        //**SET FIFO MODE
        // we only enable 1 channel for the example
        /*
        // record FIFO mode multiple recording
        bool bSpcMSetupModeRecFIFOMulti (               // returns false if error occured, otherwise true
            ST_SPCM_CARDINFO   *pstCardInfo,            // pointer to a filled card info structure
            uint64              qwChEnable,             // channel enable mask for the next acquisition
            int64               llSegmentSize,          // size of each multiple recording segment
            int64               llPostSamples,          // samples to record after trigger event for each segment
            int64               llSegmentsToRec = 0);   // numbe of segments to record in total. If zero we reun continuously
        */
        if (success && !dwError)
        {
            if(OsciModeEnArg)
            {
                success = bSpcMSetupModeRecFIFOMulti(pstCard, daqInfoPtr->chMap,(int64)SAMPLESPERPOINT , (int64)(SAMPLESPERPOINT-8),0);//note SAMPLESPERPOINT-128
                dwError=spcm_dwSetParam_i32 (pstCard->hDrv, SPC_PRETRIGGER, 8);

                updateTimerdaq->stop();
            }
            else
            {
                success = bSpcMSetupModeRecFIFOMulti(pstCard, daqInfoPtr->chMap,(int64)SAMPLESPERPOINT , (int64)(SAMPLESPERPOINT-8),burstAlignedScanPoints);
                dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_PRETRIGGER, 8);

                updateTimerdaq->start(10);
            }
        }

        //***********************************************************
        //SET TRIGGER CH-1
        /*
        channel trigger is set for each channel separately
        bool bSpcMSetupTrigChannel (                    // returns false if error occured, otherwise true
            ST_SPCM_CARDINFO   *pstCardInfo,            // pointer to a filled card info structure
            int32               lChannel,               // channel to modify
            int32               lTrigMode,              // channel trigger mode
            int32               lTrigLevel0 = 0,        // level 0
            int32               lTrigLevel1 = 0,        // level 1
            int32               lPulsewidth = 0,        // programmable pulsewidth for channel
            bool                bTrigOut = false,       // trigger output
            bool                bSingleSrc = true);     // acts as single trigger source, all other masks cleared
         * */

        {         //SET TRIGGER CH-1
            //success = bSpcMSetupTrigChannel(pstCard,1,SPC_TM_POS|SPC_TM_REARM,500,400,2000,false,true);
            //dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_CHENABLE, CHANNEL1|CHANNEL0);
        }

        //*********************************************************************
        //SET CHANNELS
        /*
        bool bSpcMSetupPathInputCh (                    // returns false if error occured, otherwise true
            ST_SPCM_CARDINFO   *pstCardInfo,            // pointer to a filled card info structure
            int32               lChannel,               // channel to change
            int32               lPath,                  // input path
            int32               lInputRange,            // input range in mV = (max-min)/2, =1000 for +/-1V range
            bool                bTerm = true,           // set input termination (50 ohm) if available
            bool                bACCoupling = false,    // AC coupling activated
            bool                bBWLimit = false,       // bandwidth limit activated
            bool                bDiffInput = false);    // set differential input if available
        */
        //note with AC coupling and hi impedence mode the inpout >2Hz
        dwDataNotify = 0;
        for (int i=0; i < pstCard->lMaxChannels; i++)
        {
            if (success && !dwError && (daqInfoPtr->chMap & (0x1<<i)))
            {
                success = bSpcMSetupPathInputCh (pstCard, i, 0, daqInfoPtr->daqVoltage, true, true, false,false); // setup for M3i card series including new features
//                dwDataNotify +=   BYTESPERSAMPLE*SAMPLESPERPOINT;
            }
        }

        //*************************************************************************
        //ALLOCATE BUFFERS and do setup for DMA

        //for increasing the buf len beyond256MB we need to increase the dwNotify size beyond 1024 as well.
        dwDataBufLen =   MEGA_B(512);
        dwDataNotify =   burstSize*BYTESPERSAMPLE*SAMPLESPERPOINT; // valid values are 16,32,64,128,256,512,1k,2k,4k and multiples of 4K

        //dwDataNotify =   BYTESPERSAMPLE*SAMPLESPERPOINT; // valid values are 16,32,64,128,256,512,1k,2k,4k and multiples of 4K
        if ( ((dwDataNotify !=  512  && dwDataNotify !=  1024 && dwDataNotify !=  2048) && (dwDataNotify < 4096)) ||
             ((dwDataNotify%4096 !=  0) && (dwDataNotify >= 4096))
             )
        {//make a warning, current samples per point are not aligned to the dma notification size.
            QMessageBox msgBox(QMessageBox::Critical, tr("Digitizer DMA setting"),tr("samples per point are not aligned to the dma notification size."), 0, this);
            qDebug()<<dwDataNotify;
            msgBox.exec();
            return;
        }

        spcm_dwGetContBuf_i64 (pstCard->hDrv, SPCM_BUF_DATA, &binaryWfmBuff, &qwContBufLen);
        bContMemUsed = (qwContBufLen >= dwDataBufLen);

        if (!bContMemUsed)
        {
            //assign your own buffer incase cont buffer is not available
            binaryWfmBuff = pvAllocMemPageAligned (dwDataBufLen);
        }

        /*// Defines the transer buffer by using 64 bit unsigned integer values
        SPCM_IMPORT uint32 _stdcall spcm_dwDefTransfer_i64 (
            drv_handle  hDevice,                // handle to an already opened device
            uint32      dwBufType,              // type of the buffer to define as listed above under SPCM_BUF_XXXX
            uint32      dwDirection,            // the transfer direction as defined above
            uint32      dwNotifySize,           // amount of bytes after which i want do receive an event (0=end of transfer)
            void*       pvDataBuffer,           // pointer to the data buffer
            uint64      qwBrdOffs,              // offset for transfer in board memory
            uint64      qwTransferLen);         // buffer length
            */
        // all is prepared and we define the buffers for the transfer
        if (success && binaryWfmBuff != NULL)
            dwError = spcm_dwDefTransfer_i64 (pstCard->hDrv, SPCM_BUF_DATA, SPCM_DIR_CARDTOPC, (uint32) dwDataNotify, binaryWfmBuff, 0, dwDataBufLen);

        if (dwError || !success)
        {
            daqErrorHandler(dwError, pstCard);
        }
        else
        {
           //read trriger counter
            qDebug()<<QDateTime::currentDateTime().time();
            qDebug("daqController::Configure -- Sampling rate set to %.1lf MHz, binaryWfmBuff: 0x%x, OscilloscopeMode: %d",
                   (double) pstCard->llSetSamplerate / 1000000,binaryWfmBuff,this->OsciModeEn);

            configFetchThread();
            dwError = spcm_dwSetParam_i32(pstCard->hDrv, SPC_TIMEOUT, 10000);
            dwError = spcm_dwSetParam_i32(pstCard->hDrv, SPC_M2CMD, M2CMD_DATA_STARTDMA);
            fetcherThread->start();
        }
    }
}

void daqController::daqErrorHandler(uint32 dwError, ST_SPCM_CARDINFO *pstCardInfo)
{

    pstCardInfo->bSetError = true;
    spcm_dwGetErrorInfo_i32 (pstCardInfo->hDrv, NULL, NULL, pstCardInfo->szError);
    qDebug()<<dwError<<pstCardInfo->szError;
    errorHandler(pstCardInfo->szError);
}

void daqController::configFetchThread()
{
  fetcherObj = new fetcher;
  fetcherThread = new QThread;
  fetcherObj->moveToThread(fetcherThread);

  connect(fetcherObj, SIGNAL(errorSignal(QString)), this, SLOT(errorHandler(QString)));
  connect(fetcherObj, SIGNAL(newArrival(int,long)), this, SLOT(newArrivalInDaq(int,long)));
  connect(fetcherObj, SIGNAL(finished(int)), this, SLOT(acqFinished(int)));
  connect(fetcherObj, SIGNAL(finished(int)), fetcherThread, SLOT(quit()));
  connect(fetcherObj, SIGNAL(finished(int)), fetcherObj, SLOT(deleteLater()));
  connect(fetcherThread, SIGNAL(finished()), fetcherThread, SLOT(deleteLater()));

  if (this->OsciModeEn==false)
     connect(fetcherThread, SIGNAL(started()), fetcherObj, SLOT(fetch()));
  else
     connect(fetcherThread, SIGNAL(started()), fetcherObj, SLOT(fetchOsciMode()));


  fetcherObj->setPars(this->pstCard,burstAlignedScanPoints,binaryWfmBuff,OsciModeEn,dwDataBufLen,this->burstSize);

}

void daqController::StopAcquisition()
{
    if (fetcherObj!=NULL)
        fetcherObj->reset();
}

void daqController::acqFinished(int lastWaveFormNum)
{
    qDebug()<<"daqController::acqFinished..";
    uint32 dwError;
    dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_M2CMD, M2CMD_DATA_STOPDMA);
    dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_M2CMD, M2CMD_CARD_INVALIDATEDATA);
    dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_M2CMD, M2CMD_ALL_STOP);
    //dwError = spcm_dwSetParam_i32 (pstCard->hDrv, SPC_M2CMD, M2CMD_CARD_RESET); better to avoid for latch life.
    if (dwError)
    {
        daqErrorHandler (dwError, pstCard);
        return;
    }

    //free memory
    if (binaryWfmBuff && !bContMemUsed)
    {
           vFreeMemPageAligned (binaryWfmBuff,dwDataBufLen );
           binaryWfmBuff = 0;
    }

    bContMemUsed = false;

    this->fetcherObj = NULL;
    if (OsciModeEn == false)
    {
        emit updateProgressBar_daqControllerSignal((double)(100));
        if (lastWaveFormNum!=burstAlignedScanPoints)
        {
            QString StatusTip = "WARNING!!! Unexpected finish of data acquistion - "+(QString::number(lastWaveFormNum) + "/" + QString::number(burstAlignedScanPoints));
            emit updateStatusBar_daqControllerSignal(StatusTip);
        }
        emit scanFinished(lastWaveFormNum); //always butterworth should be enabled
        //if(ButterWorthIIREnabled_f)
            //handleErr (niScope_ClearWaveformProcessing(vi_f, signalChannel_f));
    }
    else
    { // do nothing

    }
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

void daqController::newArrivalInDaq(int waveformNumber, long lAvailPos)
{
    short * pvDataCurrentBuff = (short*)(((char*) this->binaryWfmBuff) + lAvailPos);
    int i,temp;
    #ifdef DAQ_DEBUG_LOGS
//        if (!this->OsciModeEn)
//        {
//            qDebug()<<QDateTime::currentDateTime().time()<<
//                      "daqController::newArrivalInDaq waveformNumber:"<<waveformNumber<<
//                      "lAvailPos"<<lAvailPos<<
//                      "this->binaryWfmBuff"<<this->binaryWfmBuff<<
//                      "pvDataCurrentBuff"<<pvDataCurrentBuff;
//            }
    #endif

        if (daqInfoPtr->chMap == 3)
        {
            for (i=0;i<burstSize*SAMPLESPERPOINT;i++)
            {
                temp = ((int)pvDataCurrentBuff[2*i]+(int)pvDataCurrentBuff[(2*i)+1]);
                pvDataCurrentBuff[i] = temp>>1;
            }
        }
    if (OsciModeEn == false)
    {
        //generate signal to dataProcessor to copy the data
        emit newWfmReadyForCopy(pvDataCurrentBuff,waveformNumber,this->burstSize);
        if (allowOsciUpdate)
        {
            emit updatePlotOsci(pvDataCurrentBuff);
            allowOsciUpdate = false;
        }
        emit updateProgressBar_daqControllerSignal(((double)(waveformNumber+1)/daqInfoPtr->ScanPoints)*100);
        QString StatusTip = (QString::number(waveformNumber+1) + "/" + QString::number(daqInfoPtr->ScanPoints));
        emit updateStatusBar_daqControllerSignal(StatusTip);
    }
    else
    { //osci mode generate signal directly to plot
        emit updatePlotOsci(pvDataCurrentBuff);
        wfmCopyDone(0);//to set byte available in oscilloscope mode
    }
}
void daqController::wfmCopyDone(int waveformNumber)
{
    uint32 dwError = ERR_OK;
    int availData;
    unsigned int remainingWfms = this->burstAlignedScanPoints-waveformNumber;

    //this is alwayts constant since we will always copy 1waveform worth of data
    //dwError = spcm_dwSetParam_i32(pstCard->hDrv, SPC_DATA_AVAIL_CARD_LEN, BYTESPERSAMPLE*SAMPLESPERPOINT);
    //if (!isDemoCard)

    if (remainingWfms < burstSize)
        availData = BYTESPERSAMPLE*SAMPLESPERPOINT*remainingWfms;
    else
        availData = this->dwDataNotify;

    dwError = spcm_dwSetParam_i32(pstCard->hDrv, SPC_DATA_AVAIL_CARD_LEN, availData);
    if (this->fetcherObj != NULL)
        fetcherObj->resetWaitForCopy();


#ifdef DAQ_DEBUG_LOGS
//    if (!this->OsciModeEn)
//        qDebug()<<QDateTime::currentDateTime().time()<<"daqController::wfmCopyDone waveformNumber:"<<waveformNumber<<
//                  "dwError"<<dwError<<
//                  "availData"<<availData<<
//                  "remainingWfms"<<remainingWfms<<
//                  "burstSize"<<burstSize;

#endif

    if (dwError)
    {
        daqErrorHandler (dwError, pstCard);
        return;
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
    this->waitForCopy = true;
}

fetcher::~fetcher()
{

}

void fetcher::resetWaitForCopy()
{
    waitForCopy = false;
}

void fetcher::reset()
{
    this->reqWfmCnt_f = fetchedWfmCnt_f;
    this->OsciModeEn_f = false;
    this->waitForCopy = false;
}
void fetcher::setPars(ST_SPCM_CARDINFO* pstCard_arg, qint32 reqWfmCnt_arg, void * pvDataBuffer_arg, bool OsciModeEn_arg
                      , uint32 dwDataBufLen_arg, int burstSize_arg)
{
    pstCard_f = pstCard_arg;
    reqWfmCnt_f = reqWfmCnt_arg;
    pvDataBuffer_f = pvDataBuffer_arg;
    OsciModeEn_f = OsciModeEn_arg;
    dwDataBufLen_f = dwDataBufLen_arg;
    burstSize_f     = burstSize_arg;
}

void fetcher::fetch()
{
    uint32 dwError = ERR_OK;
    int32 lStatus = 0;
    int32 dwDataAvailBytes = 0;
    int32 lAvailPos = 0,lAvailPosPrev = -1;
    QElapsedTimer timer;
    //void *pvDataCurrentBuf = 0;

    qDebug()<<QDateTime::currentDateTime().time()<<"fetcher::fetch ready and waiting";
    spcm_dwSetParam_i32(pstCard_f->hDrv, SPC_TIMEOUT, 10000);//#note
    dwError = spcm_dwSetParam_i32 (pstCard_f->hDrv, SPC_M2CMD, M2CMD_DATA_STARTDMA|M2CMD_CARD_START | M2CMD_CARD_ENABLETRIGGER|M2CMD_CARD_WAITTRIGGER);
    /*
    dwError = spcm_dwSetParam_i32 (pstCard_f->hDrv, SPC_M2CMD, M2CMD_CARD_START | M2CMD_CARD_ENABLETRIGGER);
    dwError = spcm_dwSetParam_i32(pstCard_f->hDrv, SPC_M2CMD, M2CMD_DATA_POLL);
    dwError = spcm_dwSetParam_i32(pstCard_f->hDrv, SPC_M2CMD, M2CMD_CARD_WAITTRIGGER);

    if (dwError)
    {
        spcm_dwGetErrorInfo_i32 (pstCard_f->hDrv, NULL, NULL, pstCard_f->szError);
        qDebug()<<"fetch: ERROR"<<pstCard_f->szError;
        emit errorSignal(pstCard_f->szError);
    }
    */
//    QTime myTimer;
//    myTimer.start();

    while((fetchedWfmCnt_f < reqWfmCnt_f) && (reqWfmCnt_f > 0) && !dwError)
    {
        dwError = spcm_dwSetParam_i32 (pstCard_f->hDrv, SPC_M2CMD, M2CMD_DATA_WAITDMA);
        spcm_dwGetParam_i32 (pstCard_f->hDrv, SPC_M2STATUS, &lStatus);
        this->waitForCopy = true; //handshake signal

        // Recording complete (for setups with SPC_LOOPS != 0)?
        //if (lStatus & M2STAT_DATA_END) //DMA have stopped copying.
            //dwError = ERR_FIFOFINISHED;

        if (lStatus & M2STAT_DATA_OVERRUN)
            qDebug()<<"fetch: ERROR lStatus suggests Data Overrun, M2STAT_DATA_OVERRUN: "<<lStatus;

        // read out the current position of data buffer and recalculate it to avoid rollover

            spcm_dwGetParam_i32 (pstCard_f->hDrv, SPC_DATA_AVAIL_USER_LEN,   (int32*) &dwDataAvailBytes);
            spcm_dwGetParam_i32 (pstCard_f->hDrv, SPC_DATA_AVAIL_USER_POS,   &lAvailPos);

            //if ((lAvailPos + dwDataAvailBytes) >= dwDataBufLen_f)
            //    dwDataAvailBytes = (uint32) (dwDataBufLen - lAvailPos);

            //send the pos and availbytes to daqController
            //pvDataCurrentBuf = (void*) (((char*) pvDataBuffer_f) + lAvailPos);

            //send the pos and the current waveform-number


        // check for the error in the loop
        if (dwError)
        {
            if(dwError != ERR_OK && dwError != ERR_FIFOFINISHED && dwError != ERR_TIMEOUT )
            {
                pstCard_f->bSetError = true;
                spcm_dwGetErrorInfo_i32 (pstCard_f->hDrv, NULL, NULL, pstCard_f->szError);
                qDebug()<<"fetch: ERROR"<<pstCard_f->szError;
                emit errorSignal(pstCard_f->szError);
            }
            else
            {
                int64 spcLoops;
                spcm_dwGetParam_i64 (pstCard_f->hDrv, SPC_LOOPS,&spcLoops);

                spcm_dwGetErrorInfo_i32 (pstCard_f->hDrv, NULL, NULL, pstCard_f->szError);
                qDebug()<<"fetch: ERROR"<<pstCard_f->szError<<"spcLoops"<<spcLoops<<"dwError"<<dwError;
            }

            break;
        }
        else if (dwDataAvailBytes>0 && lAvailPosPrev != lAvailPos)
        {

            int64 trigCount;
            dwError=spcm_dwGetParam_i64 (pstCard_f->hDrv, SPC_TRIGGERCOUNTER, &trigCount);

            #ifdef DAQ_DEBUG_LOGS
//                qDebug()<<QDateTime::currentDateTime().time()<<"fetch: dwDataAvailBytes"<<dwDataAvailBytes<<
//                          "lAvailPos"<<lAvailPos<<
//                          "pvDataBuffer_f"<<pvDataBuffer_f<<
//                          "lStatus"<<lStatus<<
//                          "trigCount"<<trigCount<<
//                          "Elapsed since last waveform (ms)"<<timer.elapsed();
            #endif
                timer.start();
            emit newArrival( fetchedWfmCnt_f, (long)lAvailPos);
//            fetchedWfmCnt_f++;

            if (dwDataAvailBytes >= (BYTESPERSAMPLE*SAMPLESPERPOINT*burstSize_f) )
                fetchedWfmCnt_f = fetchedWfmCnt_f + burstSize_f ;
            else
                fetchedWfmCnt_f = fetchedWfmCnt_f + dwDataAvailBytes/(BYTESPERSAMPLE*SAMPLESPERPOINT);
            //fetchedWfmCnt_f = fetchedWfmCnt_f +4; //should be automated based on notify size

#ifndef ACTUAL_SYSTEM
            while (this->waitForCopy)
#endif
            {
#ifdef ACTUAL_SYSTEM
                QThread::usleep(1);
#else
                QThread::msleep(1);
#endif
                #ifdef DAQ_DEBUG_LOGS
                    //qDebug()<<QDateTime::currentDateTime().time()<<"fetch: Wait for copy done.";
                #endif
            }
            lAvailPosPrev = lAvailPos;
        }
    }
//    qDebug()<<"DAQ Time(ms): "<<myTimer.elapsed();

    emit finished(fetchedWfmCnt_f);
}

void fetcher::fetchOsciMode()
{
    uint32 dwError = ERR_OK;
    int32 lStatus = 0;
    int32 dwDataAvailBytes = 0;
    int32 lAvailPos = 0;
    //void *pvDataCurrentBuf = 0;

    dwError = spcm_dwSetParam_i32(pstCard_f->hDrv, SPC_TIMEOUT, 7000);
    dwError = spcm_dwSetParam_i32(pstCard_f->hDrv, SPC_M2CMD, M2CMD_DATA_STARTDMA|M2CMD_CARD_START|M2CMD_CARD_ENABLETRIGGER);


    while(OsciModeEn_f)
    {
        dwError = spcm_dwSetParam_i32 (pstCard_f->hDrv, SPC_M2CMD, M2CMD_DATA_WAITDMA);
        spcm_dwGetParam_i32 (pstCard_f->hDrv, SPC_M2STATUS, &lStatus);
        this->waitForCopy = true; //handshake signal

        // Recording complete (for setups with SPC_LOOPS != 0)?
        if (lStatus & M2STAT_DATA_END)
            dwError = ERR_FIFOFINISHED;

        // read out the current position of data buffer and recalculate it to avoid rollover

        spcm_dwGetParam_i32 (pstCard_f->hDrv, SPC_DATA_AVAIL_USER_LEN,   (int32*) &dwDataAvailBytes);
        spcm_dwGetParam_i32 (pstCard_f->hDrv, SPC_DATA_AVAIL_USER_POS,   &lAvailPos);

        //if ((lAvailPos + dwDataAvailBytes) >= dwDataBufLen)
         //   dwDataAvailBytes = (uint32) (dwDataBufLen - lAvailPos);

        //send the pos and availbytes to daqController
        //pvDataCurrentBuf = (void*) (((char*) pvDataBuffer_f) + lAvailPos);

        //send the pos and the current waveform-number

        if (dwError != ERR_OK && dwError !=ERR_TIMEOUT )
        {
            pstCard_f->bSetError = true;
            spcm_dwGetErrorInfo_i32 (pstCard_f->hDrv, NULL, NULL, pstCard_f->szError);
            qDebug()<<"fetchOsciMode: ERROR"<<pstCard_f->szError;
            emit errorSignal(pstCard_f->szError);
            break;
        }
        else if (dwDataAvailBytes>0)
        {

            #if 0
                qDebug()<<QDateTime::currentDateTime().time()<<
                          "fetchOsciMode: dwDataAvailBytes"<<dwDataAvailBytes<<
                          "lAvailPos"<<lAvailPos<<
                          "pvDataBuffer_f"<<pvDataBuffer_f;
            #endif

            emit newArrival(0, (long)lAvailPos);
            while (this->waitForCopy)
            {
            #ifdef ACTUALSYSTEM
                            QThread::usleep(1);
            #else
                            QThread::msleep(50);
            #endif
                #ifdef DAQ_DEBUG_LOGS
                    //qDebug()<<QDateTime::currentDateTime().time()<<"fetchOsciMode: Wait for copy done.";
                #endif
            }
        }
    }
    emit finished(0);
}

//dwError = spcm_dwSetParam_i32(pstCard_f->hDrv, SPC_M2CMD, M2CMD_EXTRA_POLL);
//dwError = spcm_dwSetParam_i32(pstCard_f->hDrv, SPC_M2CMD,M2CMD_CARD_FORCETRIGGER);



