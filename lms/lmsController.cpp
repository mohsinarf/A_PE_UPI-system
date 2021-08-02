#include "lmsController.h"

lmsController::lmsController(structLms *lmsInfoPtrArg, structScan *scanInfoPtrArg)
{
    lmsInfoPtr      = lmsInfoPtrArg;
    scanInfoPtr     = scanInfoPtrArg;

    init_LMS(-1,-1,90,0,0);
  //init_LMS(1,-1,0,0,0);

}

void lmsController::init_LMS(double flipX, double flipY, double phi, double x_offset, double y_offset)
{
    short ErrorCode;

    ErrorCode = load_correction_file("cor_1to1.ctb", 1, flipX, flipY, phi, x_offset, y_offset);

    QMessageBox errMsg;

    if (!ErrorCode)
    {
        errMsg.setText("LMS was initiated.");
        errMsg.exec();
        set_laser_mode(1);              // YAG mode selected
        select_cor_table(1,0);          // Scan head A = ON with cor_table #1
        set_delay_mode(0, 0, 65500, 0, 0);     // Disable varpoly,directmove3d,edgelevel,MinJumpDelay,JumpLenthLimit;
    UpdateLmsParStruct();
    }
    else
    {
        QString err = "Correction file loading error " + QString::number(ErrorCode);
        errMsg.setText(err);
    }

}

void lmsController::UpdateLmsParStruct()
{
    double bitFactor = 75720*atan(1/lmsInfoPtr->SOD.toDouble());
    double Height = 65354/bitFactor;
    double Width  = 65354/bitFactor;

    lmsInfoPtr->currentX = (int) Height/2;
    lmsInfoPtr->currentY = (int) Width/2;
    lmsInfoPtr->currentXabs = -32766 + (lmsInfoPtr->currentX)*bitFactor;
    lmsInfoPtr->currentYabs = -32766 + (lmsInfoPtr->currentY)*bitFactor;
    updatePos();
}

void lmsController::updatePos()
{
    QString returnValString;
    returnValString = QString::number(lmsInfoPtr->currentX,'f',2);
//    emit Xpos(returnValString);
    returnValString = QString::number(lmsInfoPtr->currentY,'f',2);
//    emit Ypos(returnValString);
}

void lmsController::MoveypStart()
{
    qDebug()<<"moveyp";
}
//double lmsController::mm2Bit_factorLMS(double length, double zDistance)
//{
////    double pos_MaxBit = 32767;
////    double phi_rel = 0.461;//0.4191;
//    double mm2Bit_factor = 75720*atan(length/zDistance);
//    //double mm2Bit_factor = (pos_MaxBit/phi_rel)*atan(length/zDistance);
//    return mm2Bit_factor;
//}

void lmsController::check_scanAreaLimit(double bit_scanWidth, double bit_scanHeight, double init_setXpos, double init_setYpos)
{
    //This checking method is based on the bottom-left corner point of scanner area.
    double limit_xRight = 32767;
    double limit_xLeft = -(limit_xRight - 1);
    double limit_yTop = 32767;
    double limit_yBottom = -(limit_yTop - 1);

    double current_xRight = init_setXpos + bit_scanWidth;
    double current_xLeft = init_setXpos;
    double current_yTop = init_setYpos;
    double current_yBottom = init_setYpos - bit_scanHeight;

    if (limit_yTop < current_yTop)
    {
        QString errTxt = "positive y";
        errorMsg_overScanAreaSet(errTxt);
    }
    else if (limit_yBottom > current_yBottom)
    {
        QString errTxt = "negative y";
        errorMsg_overScanAreaSet(errTxt);
    }

    if (limit_xLeft > current_xLeft)
    {
        QString errTxt = "negative x";
        errorMsg_overScanAreaSet(errTxt);
    }
    else if (limit_xRight < current_xRight)
    {
        QString errTxt = "positive x";
        errorMsg_overScanAreaSet(errTxt);
    }
}

void lmsController::errorMsg_overScanAreaSet(QString direction)
{
    QString errMsg = "Over the limit of the " + direction +"-axis!";
    QMessageBox errorMsg;
    errorMsg.critical(0,"Error", errMsg);
    errorMsg.setFixedSize(500,200);
}

void lmsController::lms_jumpAbs(short xAbs, short yAbs)
{
    set_start_list(1);      // Open the list memory #1
    set_jump_speed(50000);  // Jump speed in bits per ms
    jump_abs(xAbs,yAbs);
    qDebug() << "x = " << xAbs << " y = " << yAbs;
    set_end_of_list();
    execute_list(1);
}

void lmsController::lms_laserTiming()
{
    set_start_list(1);
    set_laser_timing(0xffff,    // half of laser signal period
                     0xffff,    // pulse widths of signal LASER1 (Q-switch)
                     0xffff,    // pulse widths of signal LASER2
                     1);    // time base; 0 corresponds to 1us
    set_scanner_delays(0,  // jump delay in 25us
                       10,  // mark delay in 10us
                       5);  // polygon delay 5us
    set_laser_delays(2,     // laser on delay in us
                     5);    // laser off delay in us
    set_jump_speed(50000.0); // jump speed in bits per ms
    set_mark_speed(250.0);  // mark speed in bits per ms
    set_end_of_list();
    execute_list(1);
}

void lmsController::algorithm1(short xLoc, short yLoc, unsigned int PIXELS, unsigned int LINES, double DotDist, unsigned int DotFreq)
{
        image graystairs = {
            xLoc, yLoc,
            DotDist,
            DotFreq,
            PIXELS,
            LINES,
        };

        while (!ImagePrint(&graystairs))
        {

        };

        return;
}

int lmsController::ImagePrint(lmsController::image *picture)
{
    double DRAGDELAY = 120;//360;//120;
    double AXELPERIOD;
    AXELPERIOD = (3*DRAGDELAY);

    static unsigned line = 0;
    static unsigned short pixel_period;
    static short xCounterBalance;

    unsigned i;
    unsigned short busy;
    long position;

    if(!line)
    {
        if(picture->dotFrequency < 2)
        {
            pixel_period = 0xffff;
        }
        else
        {
            pixel_period = (unsigned short)(100000/picture->dotFrequency);
        }
        xCounterBalance = (short)((double)((AXELPERIOD - DRAGDELAY)/
                          (double)pixel_period/10) * picture->dotDistance);
    }

    get_status(&busy, &position);
    if(busy)
        if(line & 1)
        {
            if(position >= 500000) return(0);
        }
        else
        {
            if(position < 500000) return(0);
        }

    if(line&1)
    {
        set_start_list((unsigned short)((line & 1) + 1));

        jump_abs((short)((picture->xLocus - xCounterBalance)+(picture->dotDistance)*(picture->ppl)),
                 (short)(picture->yLocus - (double)line*picture->dotDistance));

        set_pixel_line(1, pixel_period, -(picture->dotDistance), 0.0);

           for(i = 0; i < picture->ppl; i++)
        {
            set_pixel(511,0,0);
        }

        set_end_of_list();
    }
    else
    {
        set_start_list((unsigned short)((line & 1) + 1));

        jump_abs((short)(picture->xLocus - xCounterBalance),
                (short)(picture->yLocus - (double)line*picture->dotDistance));

        set_pixel_line(1, pixel_period, picture->dotDistance, 0.0);

        for(i = 0; i < picture->ppl; i++)
        {
            set_pixel(511,0,0);
        }

        set_end_of_list();
    }

    line?auto_change():execute_list(1);
    line++;
    if(line == picture->lpi)
    {
        line = 0;

        return(1);
    }
    return(0);
}
