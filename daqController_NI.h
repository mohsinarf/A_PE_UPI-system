#ifndef DAQCONTROLLER_H
#define DAQCONTROLLER_H

#include <QDialog>
#include <QtCore>
#include <QtGui>
#include <QDebug>
#include <QMessageBox>
#include "structDef.h"
#if 0
#include "niScope.h"
#endif

#include "dlltyp.h"
#include "regs.h"
#include "spcerr.h"
#include "spcm_drv.h"

// ----- include of common example librarys -----
#include "spcm_lib_card.h"
#include "spcm_lib_data.h"



class fetcher : public QObject
{
    Q_OBJECT

public:
    fetcher();
    ~fetcher();

public slots:
    void fetch();
    void fetchOsciMode();
#if 0
    void setPars(ViSession vi_f, ViConstString signalChannel_f,ViInt32 actualRecordLength_f,
                 double  *WfmPtr_f,struct niScope_wfmInfo *wfmInfoPtr_f, qint32 reqWfmCnt_f,
                 bool ButterWorthIIREnabled_f,short x_length_f, bool OsciModeEn_f=false);
#endif
    void reset();

signals:
    void newArrival(qint32);
    void errorSignal(QString err);
    void finished(int);

private:
#if 0
    ViSession vi_f;
    ViConstString signalChannel_f;
    ViInt32 actualRecordLength_f;
#endif
    double  *WfmPtr_f;
    struct niScope_wfmInfo *wfmInfoPtr_f;
    unsigned int  reqWfmCnt_f;//total number of waveforms to be acquired based on scan settings.
    unsigned int  fetchedWfmCnt_f; //Wavforms read so far.
    bool ButterWorthIIREnabled_f;
    short x_length_f;
    bool FirstTime;
    QTimer timer;
    bool OsciModeEn_f;

};

class daqController : public QObject
{
    Q_OBJECT
public:
    explicit daqController(structDaq *daqInfoPtrArg = 0, structScan *scanInfoPtrArg = 0);
    ~daqController();

    //void testConfigureAndFetch();
    void Configure(bool OsciModeEn=false);
    void StartAcquisition();
    void StopAcquisition();
    bool OsciModeEn;

public slots:
    void errorHandler(QString);
    void newArrivalInDaq(qint32);
    void acqFinished(int);
    void setallowOsciUpdate();

private:
    structDaq *daqInfoPtr;
    structScan *scanInfoPtr;

    #if 0
    ViSession vi;
    ViConstString signalChannel;
    ViInt32 actualRecordLength;
#endif
    short* binaryWfm;
    double* filteredWfm;
    struct niScope_wfmInfo *wfmInfoPtr;


    fetcher *fetcherObj;
    QThread* fetcherThread ;
    bool ConfigDone;
    bool allowOsciUpdate;

    QElapsedTimer timer;
    QTimer *updateTimerdaq;

    void getHandle();
    void configFetchThread();

signals:
    void updateProgressBar_daqControllerSignal(int Percentage);
    void updateStatusBar_daqControllerSignal(QString StatusTip);
    void newWfmReadyForCopy(double *WfmPointer,int waveformNumber);
    void scanFinished(int);
    void updatePlotOsci(double *);
};



#endif // DAQCONTROLLER_H
