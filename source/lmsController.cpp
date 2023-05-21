#include "lmsController.h"

lmsController::lmsController(structLms *lmsInfoPtrArg, structScan *scanInfoPtrArg)
{
//    scanThread      = new lmsThread(this,(structLms *)&lmsInfoPtrArg,(structScan *)&scanInfoPtrArg);
    lmsInfoPtr      = lmsInfoPtrArg;
    scanInfoPtr     = scanInfoPtrArg;
    //lmsInitialize();
}

void lmsController::lmsInitialize()
{
  //init_LMS(-1,-1,90,0,0);
  //init_LMS(1,-1,0,0,0);

    double flipX = 1;//-1
    double flipY = 1;//-1
    double phi = 90;
    double x_offset = 0;
    double y_offset = 0;

    short ErrorCode;

    ErrorCode = load_correction_file("Cor_1to1.ctb", 1, flipX, flipY, phi, x_offset, y_offset);

    QMessageBox errMsg;

    if (!ErrorCode)
    {
        qDebug()<<"LMS initialized";
//        errMsg.setText("LMS was initiated.");
//        errMsg.exec();
        set_laser_mode(1);              // YAG mode selected
        select_cor_table(1,0);          // Scan head A = ON with cor_table #1
        set_delay_mode(0, 0, 65500, 0, 0);     // Disable varpoly,directmove3d,edgelevel,MinJumpDelay,JumpLenthLimit;65500
    }
    else
    {
        QString err = "Correction file loading error " + QString::number(ErrorCode);
        errMsg.setText(err);
        qDebug()<<err;
    }
    UpdateLmsParStruct();
}

void lmsController::UpdateLmsParStruct()
{
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());
    double Height = 65354/bitFactor;
    double Width  = 65354/bitFactor;

    lmsInfoPtr->currentX = (int) Height/2;
    lmsInfoPtr->currentY = (int) Width/2;
    lmsInfoPtr->currentXabs = -32766 + (lmsInfoPtr->currentX)*bitFactor;
    lmsInfoPtr->currentYabs = -32766 + (lmsInfoPtr->currentY)*bitFactor;
    updatePos();
    lmsJumpAbs(lmsInfoPtr->currentXabs,lmsInfoPtr->currentYabs);
}

void lmsController::updatePos()
{
    QString returnValString;
    returnValString = QString::number(lmsInfoPtr->currentX);
    emit Xpos(returnValString);
    returnValString = QString::number(lmsInfoPtr->currentY);
    emit Ypos(returnValString);
}

void lmsController::lmsSetPos(int xLoc, int yLoc)
{
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());
    lmsInfoPtr->currentY = yLoc;
    lmsInfoPtr->currentX = xLoc;
    lmsInfoPtr->currentXabs = -32766 + (lmsInfoPtr->currentX)*bitFactor;
    lmsInfoPtr->currentYabs = -32766 + (lmsInfoPtr->currentY)*bitFactor;
    lmsAreaLimitCheck();
    lmsJumpAbs(lmsInfoPtr->currentXabs,lmsInfoPtr->currentYabs);
}

void lmsController::lmsMoveyp()
{
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());
    lmsInfoPtr->currentY += lmsInfoPtr->laserStep;
    lmsInfoPtr->currentYabs += lmsInfoPtr->laserStep * bitFactor;
    lmsAreaLimitCheck();
    lmsJumpAbs(lmsInfoPtr->currentXabs,lmsInfoPtr->currentYabs);
}

void lmsController::lmsMoveyn()
{
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());
    lmsInfoPtr->currentY -= lmsInfoPtr->laserStep;
    lmsInfoPtr->currentYabs -= lmsInfoPtr->laserStep * bitFactor;
    lmsAreaLimitCheck();
    lmsJumpAbs(lmsInfoPtr->currentXabs,lmsInfoPtr->currentYabs);
}

void lmsController::lmsMovexp()
{
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());
    lmsInfoPtr->currentX += lmsInfoPtr->laserStep;
    lmsInfoPtr->currentXabs += lmsInfoPtr->laserStep * bitFactor;
    lmsAreaLimitCheck();
    lmsJumpAbs(lmsInfoPtr->currentXabs,lmsInfoPtr->currentYabs);
}

void lmsController::lmsMovexn()
{
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());
    lmsInfoPtr->currentX -= lmsInfoPtr->laserStep;
    lmsInfoPtr->currentXabs -= lmsInfoPtr->laserStep * bitFactor;
    lmsAreaLimitCheck();
    lmsJumpAbs(lmsInfoPtr->currentXabs,lmsInfoPtr->currentYabs);
}

void lmsController::lmsDisplayArea()
{
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());

    double xAbs = lmsInfoPtr->currentXabs;
    double xxAbs = 0;
    double yAbs = lmsInfoPtr->currentYabs;
    double yyAbs = 0;
    double bit_scanInterval = scanInfoPtr->scanInterval*bitFactor;
    int scan_Point_H = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;
    int scan_Point_W = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    double TempX = lmsInfoPtr->currentX;
    double TempY = lmsInfoPtr->currentY;

    do{
        for(int i = 0; i < scan_Point_W/2; i++)
        {
            lmsJumpAbs(xAbs + i*(bit_scanInterval*2),yAbs);
            lmsInfoPtr->currentX += 2;
            xxAbs = xAbs + i*(bit_scanInterval*2);
            updatePos();
        }
        for(int j = 0; j < scan_Point_H/2; j++)
        {
            lmsJumpAbs(xxAbs,yAbs - j*(bit_scanInterval*2));
            yyAbs = yAbs - j*(bit_scanInterval*2);
            lmsInfoPtr->currentY -= 2;
            updatePos();
        }

        for(int k = 0; k < scan_Point_W/2; k++)
        {
            lmsJumpAbs(xxAbs - k*(bit_scanInterval*2), yyAbs);
            lmsInfoPtr->currentX -= 2 ;
            updatePos();
        }
        for(int l = 0; l < scan_Point_H/2; l++)
        {
            lmsJumpAbs(xAbs, yyAbs + l*(bit_scanInterval*2));
            lmsInfoPtr->currentY += 2 ;
            updatePos();
        }

        QCoreApplication::processEvents();
       lmsInfoPtr->currentX = TempX;
       lmsInfoPtr->currentY = TempY;

    }while(lmsInfoPtr->diplayArea);
}

void lmsController::lmsAreaLimitCheck()
{
    //This checking method is based on the bottom-left corner point of scanner area.
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());//75720//84053
    double limit_xRight = 32767;
    double limit_xLeft = -(limit_xRight - 1);
    double limit_yTop = 32767;
    double limit_yBottom = -(limit_yTop - 1);

    double bit_scanHeight = bitFactor*scanInfoPtr->scanHeight;
    double bit_scanWidth = bitFactor*scanInfoPtr->scanWidth;

    double current_xRight = lmsInfoPtr->currentXabs + bit_scanWidth;
    double current_xLeft = lmsInfoPtr->currentXabs;
    double current_yTop = lmsInfoPtr->currentYabs;
    double current_yBottom = lmsInfoPtr->currentYabs - bit_scanHeight;

    if (limit_yTop < current_yTop)
    {
        QString errTxt = "positive y";
        lmsScanAreaError(errTxt);
    }
    else if (limit_yBottom > current_yBottom)
    {
        QString errTxt = "negative y";
        lmsScanAreaError(errTxt);
    }

    if (limit_xLeft > current_xLeft)
    {
        QString errTxt = "negative x";
        lmsScanAreaError(errTxt);
    }
    else if (limit_xRight < current_xRight)
    {
        QString errTxt = "positive x";
        lmsScanAreaError(errTxt);
    }
}

void lmsController::lmsScanAreaError(QString direction)
{
    QString errMsg = "Over the limit of the " + direction +"-axis!";
    QMessageBox errorMsg;
    errorMsg.critical(0,"Error", errMsg);
    errorMsg.setFixedSize(500,200);
}

void lmsController::lmsJumpAbs(short xAbs, short yAbs)
{
    set_start_list(1);      // Open the list memory #1
    set_jump_speed(50000);  // Jump speed in bits per ms
    jump_abs(xAbs,yAbs);
    qDebug() << "x = " << xAbs << " y = " << yAbs;
                ;
    set_end_of_list();
    execute_list(1);
    updatePos();
}

void lmsController::lmsLaserTiming()
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

    qDebug()<<"lmsLaserTiming";
}

void lmsController::lmsAlgorithm(short xLoc, short yLoc, unsigned int PIXELS, unsigned int LINES, double DotDist, unsigned int DotFreq)
{
        strucLmsImage graystairs = {
            xLoc, yLoc,
            DotDist,
            DotFreq,
            PIXELS,
            LINES,
        };
        scanInfoPtr->quitScan= false;
        while (!lmsImagePrint(&graystairs, scanInfoPtr->quitScan) )//CHK
        {

        };

        return;
}

int lmsController::lmsImagePrint(strucLmsImage *picture, bool quitScan)
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

    if(quitScan)
    {
        line = 0;

        return(1);
    }

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
            set_pixel(400,0,0);//### change here for duty cycle
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
            set_pixel(400,0,0);//### change here for duty cycle
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

void lmsController::lmsScan()
{
    qDebug()<<"lmsScan";
    double bitFactor = 84053*atan(1/lmsInfoPtr->SOD.toDouble());
    double x = lmsInfoPtr->currentXabs;
    double y = lmsInfoPtr->currentYabs;


    short xLoc = (short)x;
    short yLoc = (short)y;
    double dotDistance = bitFactor*scanInfoPtr->scanInterval;

    int LINES = (int)((scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1);
    int PIXELS = (int)((scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1);

    int dotFrequency = (int)(1000*scanInfoPtr->PRF.toDouble());
    lmsLaserTiming();

    lmsAlgorithm(xLoc, yLoc, PIXELS, LINES, dotDistance , dotFrequency);

    emit finished();
}
