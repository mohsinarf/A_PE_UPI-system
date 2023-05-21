#ifndef LMSCONTROLLER_H
#define LMSCONTROLLER_H

#include "SCANalone4impl.h"
#include "structDef.h"
#include <QThread.h>
#include <QObject>
#include <QMessageBox>
#include <QDebug>
#include <QCoreApplication>

class lmsController : public QObject
{
    Q_OBJECT
public:
    explicit lmsController(structLms *lmsInfoPtrArg = 0, structScan *scanInfoPtrArg = 0);

public slots:

    void lmsMoveyp();
    void lmsMoveyn();
    void lmsMovexp();
    void lmsMovexn();
    void lmsScan();
    void lmsInitialize();
    void lmsScanAreaError(QString direction);
    void lmsJumpAbs(short xAbs, short yAbs);
    void lmsDisplayArea();
    void lmsAreaLimitCheck();
    void lmsAlgorithm(short xLoc, short yLoc, unsigned int PIXELS, unsigned int LINES, double DotDist, unsigned int DotFreq);
    void lmsLaserTiming();
    void lmsSetPos(int xLoc, int yLoc);

    int lmsImagePrint(strucLmsImage *picture, bool quitScan);

private:
    structLms *lmsInfoPtr;
    structScan *scanInfoPtr;

    void UpdateLmsParStruct();
    void updatePos();

signals:
    void Xpos(QString);
    void Ypos(QString);
    void finished();
};

#endif // LMSCONTROLLER_H
