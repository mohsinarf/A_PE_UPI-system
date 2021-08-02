#ifndef LMSCONTROLLER_H
#define LMSCONTROLLER_H

#include "SCANalone4impl.h"
#include "structDef.h"

#include <QObject>
#include <QMessageBox>
#include <QDebug>


class lmsController : public QObject
{
    Q_OBJECT
public:
    explicit lmsController(structLms *lmsInfoPtrArg = 0, structScan *scanInfoPtrArg = 0);
//    ~lmsController();


    struct image {
        short xLocus, yLocus;
        double dotDistance;
        unsigned dotFrequency;
        unsigned int ppl;
        unsigned int lpi;
    };

    structLms *lmsInfoPtr;
    structScan *scanInfoPtr;

    int ImagePrint(image *picture);

public slots:
    void MoveypStop();
    void MoveynStop();
    void MovexpStop();
    void MovexnStop();

    void MoveypStart();
    void MoveynStart();
    void MovexpStart();
    void MovexnStart();

    void init_LMS(double flipX, double flipY, double phi, double x_offset, double y_offset);
    void check_scanAreaLimit(double bit_scanWidth, double bit_scanHeight, double init_setXpos, double init_setYpos);
    void errorMsg_overScanAreaSet(QString direction);
    void lms_jumpAbs(short xAbs, short yAbs);
    void lms_laserTiming();
    void algorithm1(short xLoc, short yLoc, unsigned int PIXELS, unsigned int LINES, double DotDist, unsigned int DotFreq);
//    double mm2Bit_factorLMS(double length, double zDistance);

private:
    void UpdateLmsParStruct();
    void updatePos();

//signals:
//    void Xpos(QString);
//    void Ypos(QString);
};


#endif // LMSCONTROLLER_H

