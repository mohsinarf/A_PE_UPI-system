#ifndef DAQCONTROLLERF_H
#define DAQCONTROLLERF_H

#include <QDialog>
#include <qDebug>
//#include <QTimer>
#include <QMessageBox>
#include "structDef.h"
#include "fpga_lib/main_uwpi.h"
#include "fpga_lib/fwutils.h"
#include "fpga_lib/C4DSPBlast.h"
#include "fpga_lib/cid.h"
#include "fpga_lib/fmc10x.h"
#include "fpga_lib/fmc10xids.h"

class daqControllerF : public QWidget
{
    Q_OBJECT
public:
    explicit daqControllerF(structDaq *daqInfoPtrArg = 0, structScan *scanInfoPtrArg = 0);
//    ~daqControllerF();

    #define FMC104_ID	0x3E
    int Configure();
    int StartAcquisition();
    void StopAcquisition();
    bool OsciModeEn;
    bool ConfigDone;
    bool allowOsciUpdate;
    uint32_t g_AddrSipFMC10x;

signals:
//    int Configure();
//    int StartAcquisition();
//    void StopAcquisition();
    //bool OsciModeEn;

private:
    structDaq *daqInfoPtr;
    structScan *scanInfoPtr;
    void* binaryWfmBuff;
//    QTimer *updateTimerdaq;

public slots:
//    void acqFinished(int);
//    void setallowOsciUpdate();
//    void wfmCopyDone(int);
};

#endif // DAQCONTROLLERF_H
