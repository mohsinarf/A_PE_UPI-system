#include "spectrogram.h"
#include "qwt_symbol.h"

#define WIDTHOFYAXISANDCOLOURBAR            150
#define WIDTHOFYAXISANDCOLOURBARNOTITLE     124
#define HEIGHTOFBOTTOMXAXIS                 40
#define DSTBWSPECT  160
#define DSTBWSPECTPOTRAIT  40
#define CANSIZE     600

spectrogram::spectrogram(QWidget *parent, structScan * scanInfoPtr_arg ,
                         short leftPosArg, short topPosArg, short widthArg, short heightArg,
                         QString titleArg, int noOfSlavesArg):
    QwtPlot( parent )
{
    this->scanInfoPtr = scanInfoPtr_arg;

    int y_length = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;
    int x_length = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    x_length = y_length = 51;
    this->enableAxis(QwtPlot::yLeft, true);
    scaleDrawXaxis = new MyScaleDraw;
    scaleDrawYaxis = new MyScaleDraw;
    if (scanInfoPtr->enableTT == true)
        this->axisScaleEngine(QwtPlot::yLeft)->setAttribute(QwtScaleEngine::Inverted,true);

    this->setAxisScaleDraw(QwtPlot::xBottom,scaleDrawXaxis);
    this->setAxisScaleDraw(QwtPlot::yLeft,scaleDrawYaxis);

    QwtPlotPicker* picker=new QwtPlotPicker(canvas());
    picker->setStateMachine(new QwtPickerClickPointMachine);
    picker->setMousePattern(QwtPicker::MouseSelect1,Qt::MiddleButton);
    connect(picker,SIGNAL(selected(QPointF)),SLOT(selectPoint(QPointF)));

    d_spectrogram = new QwtPlotSpectrogram();
    d_spectrogram->setRenderThreadCount( 1 ); // use system specific thread count

    d_spectrogram->setColorMap( new ColorMap() );
    d_spectrogram->setCachePolicy( QwtPlotRasterItem::NoCache );

    data = new mydata(500,y_length,x_length);
    d_spectrogram->setData(data);
    d_spectrogram->attach( this );
    setcolorbar();

    plotLayout()->setAlignCanvasToScales( true );
    zoomer = new SpectZoomer( canvas() );
    zoomer->setMousePattern( QwtEventPattern::MouseSelect2,
        Qt::RightButton, Qt::ControlModifier );
    zoomer->setMousePattern( QwtEventPattern::MouseSelect3,
        Qt::RightButton );

    QwtPlotPanner *panner = new QwtPlotPanner( canvas() );
    panner->setAxisEnabled( QwtPlot::yRight, false );
    panner->setMouseButton( Qt::MidButton );

    // Avoid jumping when labels with more/less digits
    // appear/disappear when scrolling vertically
    const QColor c( Qt::darkBlue );
    zoomer->setRubberBandPen( c );
    zoomer->setTrackerPen( c );


    zoomer->zoom(QRectF::QRectF(0,0,x_length,y_length));
    zoomer->setZoomBase(QRectF::QRectF(0,0,x_length,y_length));

    qDebug()<<"spectrogram::spectrogram - x_length"<<x_length<<"y_length"<<y_length
           <<"canvasWidth"<<canvas()->width()
           <<"canvasHeight"<<canvas()->height();

    leftPos = leftPosArg;
    topPos  = topPosArg;

    width    = CANSIZE + WIDTHOFYAXISANDCOLOURBARNOTITLE;
    height   = CANSIZE + HEIGHTOFBOTTOMXAXIS;

    this->imgHeight = height;
    this->imgWidth  = width;

    this->setGeometry(QRect(leftPos,topPos,width,height));
    this->updateGeometry();

    title = titleArg;
    this->setTitle(title);
    replot();

    imageForMovie = NULL;
    painterForMovie = NULL;

    container        = parent;
    noOfSlaves       = noOfSlavesArg;
    isIntData        = false;
    isGreyForUWPI    = false;
    enlargeEnabled   = false;
    //polygon based area calculations.
    boundaryCurve = new QwtPlotCurve();
    boundaryCurve->setStyle( QwtPlotCurve::Lines );
    boundaryCurve->setPen( canvas()->palette().color( QPalette::WindowText ) );
    boundaryCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
    boundaryCurve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
    boundaryCurve->setPaintAttribute( QwtPlotCurve::FilterPoints, true );
    boundaryCurve->setSymbol( new QwtSymbol( QwtSymbol::Diamond,Qt::gray, c, QSize( 8, 8 ) ) );
    boundaryCurve->attach( this );

    QwtPlotPicker* pickerPoly=new QwtPlotPicker(canvas());
    pickerPoly->setStateMachine(new QwtPickerDragPointMachine);
    pickerPoly->setMousePattern(QwtPicker::MouseSelect1,Qt::LeftButton,Qt::ControlModifier);
    connect(pickerPoly,SIGNAL(selected(QPointF)),SLOT(selectPointForPolygon(QPointF)));
    connect(pickerPoly,SIGNAL(moved(QPointF)),SLOT(selectPointForPolygon(QPointF)));

    QwtPlotPicker* pickerPolyClear=new QwtPlotPicker(canvas());
    pickerPolyClear->setStateMachine(new QwtPickerClickPointMachine);
    pickerPolyClear->setMousePattern(QwtPicker::MouseSelect1,Qt::RightButton,Qt::ControlModifier);
    connect(pickerPolyClear,SIGNAL(selected(QPointF)),SLOT(clearPolyGon(QPointF)));
}

void spectrogram::clearPolyGon( QPointF Pos )
{
    pointList.clear();
    boundaryCurve->setSamples(pointList);

    int indexOfArea = this->title.indexOf("  -  s",0);
    if ( indexOfArea != -1) // appending the area string for the first time
        this->title.truncate(indexOfArea);
    setTitle(this->title);

    replot();
}

void spectrogram::selectPointForPolygon( QPointF Pos )
{
    int PosXInt,PosYInt;
    QPointF intPt;
    float area;

    PosXInt = Pos.x();
    PosYInt = Pos.y();

    intPt.setX( PosXInt/*+ scanInfoPtr->scanInterval/2*/ );
    intPt.setY( PosYInt/* + scanInfoPtr->scanInterval/2*/);

    //copy the first point twice to ensure closed polygon
    if (pointList.isEmpty())
    {
        pointList.append(Pos);
        pointList.append(Pos);
    }
    else
    {
        pointList.insert(pointList.size()-1,Pos);
    }

    qDebug() << "selectPoint - Float("<<Pos.x() <<","<< Pos.y()<<")"
             <<"Int("<< PosXInt << PosYInt <<")"
            <<"Pos.toPoint()"<<Pos
    <<"isClosed ?" <<pointList.isClosed()
    <<"pointListSize"<<pointList.size();

    //populate the curve
    boundaryCurve->setSamples(pointList);
    replot();

    //calculate area
    if (pointList.isClosed())
    {
        area = polyArea();

        QString areaString = "  -  selected area = "+QString::number(area,'f',2) + " mm" + QString(QChar(178));
        int indexOfArea = this->title.indexOf("  -  s",0);
        if ( indexOfArea != -1) // appending the area string for the first time
            this->title.truncate(indexOfArea);
        this->title = this->title + areaString;

        this->setTitle(this->title);
    }
}

float spectrogram::polyArea()
{
    float area = 0;

    for (int i = 0;i<(pointList.size()-1);i++) //last point is repeation of first point.
        area += (pointList[i].x()*pointList[i+1].y()) - (pointList[i+1].x()*pointList[i].y());

    area = scanInfoPtr->scanInterval*scanInfoPtr->scanInterval*area/2;

    return (area<0 ? -1*area : area);
}

void spectrogram::selectPoint( QPointF Pos )
{
    int PosXInt,PosYInt,SelectedImpingePoint;

    int y_length = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;
    int x_length = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    QPointF intPt;

    PosXInt = Pos.x();
    PosYInt = Pos.y();
    SelectedImpingePoint =   (PosYInt)  + (y_length)*(PosXInt);

    if ( PosXInt >= 0 && PosYInt >= 0 && PosXInt < x_length && PosYInt < y_length)
    {
        //emit signal to plot for plotting
        emit pointToPlot(SelectedImpingePoint);
    }

    return;
}

spectrogram::~spectrogram()
{
    delete d_spectrogram;
    //delete data;
    delete zoomer;
    if (imageForMovie!=NULL)
        delete imageForMovie;
}

void spectrogram::updateAxisXY(int enlargeFactor)
{
    container = this->parentWidget();

    int y_length = (scanInfoPtr->scanHeight/scanInfoPtr->scanInterval)+1;
    int x_length = (scanInfoPtr->scanWidth/scanInfoPtr->scanInterval)+1;
    int DefaultSize;

    data->x_length = x_length;
    data->y_length = y_length;
    data->updateDataAxis();


    zoomer->zoom(QRectF::QRectF(0,0,x_length,y_length));
    zoomer->setZoomBase(QRectF::QRectF(0,0,x_length,y_length));
    zoomer->scanInterval = scanInfoPtr->scanInterval;
    zoomer->x_length    =   x_length; // just for debug

    //re-draw scale
    scaleDrawXaxis->scanInterval = scanInfoPtr->scanInterval;
    scaleDrawYaxis->scanInterval = scanInfoPtr->scanInterval;

    scaleDrawYaxis->invalidateCacheLoc();
    scaleDrawXaxis->invalidateCacheLoc();
    axisWidget(yLeft)->update();
    axisWidget(xBottom)->update();

    float HtoWratio = (float)y_length/(float)x_length; //YtoXRatio
    float WtoHratio = (float)x_length/(float)y_length;
    int newWidth=width,newHeight=height;
    if (enlargeEnabled)
    {
        double enlargeMultiplier = 1.0;
        enlargeMultiplier = pow(2,enlargeFactor);
        int inchWidth = (x_length/25.4)*(logicalDpiX()*1.41);
        int inchHeight = (y_length/25.4)*(logicalDpiY()*1.41);

        newWidth    = inchWidth;
        newHeight   = inchHeight;

        newWidth*=enlargeMultiplier;
        newHeight*=enlargeMultiplier;

        newWidth    += WIDTHOFYAXISANDCOLOURBARNOTITLE;
        newHeight   += HEIGHTOFBOTTOMXAXIS;

        if (newWidth<6000 && newHeight<6000)
        {
            container->setGeometry(QRect(leftPos,topPos, newWidth, newHeight ));
            container->updateGeometry();
            setGeometry(QRect(leftPos,topPos, newWidth, newHeight));
        }
        else
        {
            QMessageBox msgBox(QMessageBox::Information, tr("Size Exceeds Limits"),tr("This is not supported.Please choose a smaller size."), 0, this);
            msgBox.exec();
        }
    }
    else
    {

        if (y_length<x_length)
        {
            if (HtoWratio >= 0.5) // 0.625 coz inverse is clean
            {   
                newHeight   = (HtoWratio)*(CANSIZE) + (HEIGHTOFBOTTOMXAXIS);
            }
            else
            {
                newWidth    = (0.5)*(WtoHratio)*(CANSIZE) + (WIDTHOFYAXISANDCOLOURBARNOTITLE);
                newHeight   = (0.5)*(CANSIZE) + (HEIGHTOFBOTTOMXAXIS);
            }
        }
        else if (x_length<y_length)
        {
            if (WtoHratio >= 0.9) // 0.625 coz inverse is clean
            {   //only decrease the width
                newWidth    = (WtoHratio)*(CANSIZE) + (WIDTHOFYAXISANDCOLOURBARNOTITLE);
            }
            else
            {
                newWidth    = (0.9)*(CANSIZE) + (WIDTHOFYAXISANDCOLOURBARNOTITLE);
                newHeight   = (0.9)*(HtoWratio)*(CANSIZE) + (HEIGHTOFBOTTOMXAXIS);
            }
        }
        setGeometry(QRect(leftPos,topPos, newWidth, newHeight));
    }
    if (noOfSlaves > 0 && !enlargeEnabled)//master sets the container below itself
    {
        if(newWidth > (CANSIZE + WIDTHOFYAXISANDCOLOURBARNOTITLE) ) //square aspect widthm= CANSIZE + WIDTHOFYAXISANDCOLOURBARNOTITLE
        {
            container->setGeometry(QRect(leftPos,topPos, newWidth, (noOfSlaves+1)*(newHeight+DSTBWSPECTPOTRAIT)-DSTBWSPECTPOTRAIT ));
            container->updateGeometry();
            emit placeSlave(leftPos, topPos+newHeight+DSTBWSPECTPOTRAIT);
        }
        else //save at side of master
        {
            container->setGeometry(QRect(leftPos,topPos, (noOfSlaves+1)*(newWidth+DSTBWSPECT)-DSTBWSPECT, newHeight ));
            container->updateGeometry();
            emit placeSlave(leftPos+newWidth+DSTBWSPECT, topPos);
        }
    }

    this->imgHeight = newHeight;
    this->imgWidth  = newWidth;

    setcolorbar();
    this->updateGeometry();
    this->setTitle(title);
    replot();
    repaint();

    qDebug()<<this->objectName()<<"noOfSlavesArg"<<noOfSlaves<<"spectrogram::updateAxisXY - x_length"<<x_length<<"y_length"<<y_length
           <<"canvasWidth"<<canvas()->width()
           <<"canvasHeight"<<canvas()->height()
          <<"newWidth"<<newWidth
          <<"newHeight"<<newHeight;

}

void spectrogram::placeSlaveslot(int leftPos, int topPos)
{
    this->leftPos = leftPos;
    this->topPos = topPos;

    this->updateAxisXY();
}


void spectrogram::showSpectrogram( bool on )
{
    d_spectrogram->setDisplayMode( QwtPlotSpectrogram::ImageMode, on );
    d_spectrogram->setDefaultContourPen(on ? QPen( Qt::black, 0 ) : QPen( Qt::NoPen ) );
    replot();
}

void spectrogram::setIntensity( int alpha )
{
    data->intensity = alpha;
    data->updateDataAxis();
    setcolorbar();
    plotLayout()->setAlignCanvasToScales( true );
    replot();
}

void spectrogram::toggleUWPIGreyScale(bool isGreyForUWPI)
{
    this->isGreyForUWPI = isGreyForUWPI;
    setcolorbar();
    updateAxisXY();
}

void spectrogram::setcolorbar()
{
    const QwtInterval zInterval = d_spectrogram->data()->interval( Qt::ZAxis );
    // A color bar on the right axis
    QwtScaleWidget *rightAxis = axisWidget( QwtPlot::yRight );
    rightAxis->setColorBarEnabled( true );

    if (this->isIntData == false)
    {
        if(isGreyForUWPI)
        {
            rightAxis->setColorMap( zInterval, new ColorMapGrey() );
            d_spectrogram->setColorMap(new ColorMapGrey());
        }
        else
        {
            rightAxis->setColorMap( zInterval, new ColorMap()  );
            d_spectrogram->setColorMap( new ColorMap() );
        }
    }
    else
    {
        if(isGreyForUWPI)
        {
            rightAxis->setColorMap( zInterval, new ColorMapGrey() );
            d_spectrogram->setColorMap(new ColorMapGrey());
        }
        else
        {

            rightAxis->setColorMap( zInterval, new VTWAMColorMap() );
            d_spectrogram->setColorMap( new VTWAMColorMap());
        }
    }

    setAxisScale( QwtPlot::yRight, zInterval.minValue(), zInterval.maxValue() );
    enableAxis( QwtPlot::yRight );
}


void spectrogram::savePlot(QString outfolderpath)
{
    QFont tempFont;
    float origFontSize[4];
    float origFontSizeTitle;
    QwtText tempText;
    short scaleWindowSize=200;
    float Ratio,fontReductionFactor,wReductionRatio,hAfterScaled,hReductionRatio,wAfterScaled;

    //how much reduction will be done while fitting?
    if (this->imgWidth>this->imgHeight)
    {
        Ratio = this->imgWidth/this->imgHeight;
        wReductionRatio = this->imgWidth/scaleWindowSize;
        hAfterScaled = this->imgHeight/wReductionRatio;

        if (hAfterScaled < 30)
        {
            wReductionRatio = this->imgHeight/30;
            scaleWindowSize = this->imgWidth/wReductionRatio;
        }

        //fontReductionRatio = Ratio*(float)(200.0/(float)scaleWindowSize);

        fontReductionFactor = 1;
        if (Ratio>2 && Ratio < 4)
            fontReductionFactor = 1.5;
        else if (Ratio>4)
            fontReductionFactor = 1.75;

    }
    else
    {
        Ratio = this->imgHeight/this->imgWidth;

        hReductionRatio = this->imgHeight/scaleWindowSize;
        wAfterScaled = this->imgWidth/hReductionRatio;

        if (wAfterScaled < 30)
        {
            hReductionRatio = this->imgWidth/30;
            scaleWindowSize = this->imgHeight/hReductionRatio;
        }

        fontReductionFactor = 1;
        if (Ratio>2 && Ratio < 4)
            fontReductionFactor = 1.5;
        else if (Ratio>4)
            fontReductionFactor = 1.75;
    }

    for (int i = 0;i<3;i++)
    {
        origFontSize[i] = this->axisFont(i).pointSizeF();
        tempFont.setPointSize(origFontSize[i]/fontReductionFactor);
        this->setAxisFont(i,tempFont);
    }

    qDebug()<<"spectrogram::savePlot"<<
              "this->imgWidth"<<this->imgWidth<<
              "this->imgHeight"<<this->imgHeight<<
              "origFontSize"<<origFontSize[0]<<
              "reductionRatio"<<Ratio<<
              "New Font Size"<<origFontSize[0]/Ratio+2<<
              "origFontSizeTitle"<<origFontSizeTitle;



    QSizeF t3(this->imgWidth,this->imgHeight);
    t3.scale(scaleWindowSize, scaleWindowSize,Qt::KeepAspectRatio);

    renderer.renderDocument(this,outfolderpath+".jpeg","jpeg",t3,500);

    for (int i = 0;i<3;i++)
    {
        tempFont.setPointSize(origFontSize[i]);
        this->setAxisFont(i,tempFont);
    }
}

void spectrogram::setMovieImageSize()
{
    QFont tempFont;
    float origFontSize[4];
    short scaleWindowSize=100;
    float Ratio,fontReductionFactor,wReductionRatio,hAfterScaled,hReductionRatio,wAfterScaled;

    const double mmToInch = 1.0 / 25.4*1.41;
    const int resolution = 100;
    if (this->scanInfoPtr->scanHeight > 599 ||
        this->scanInfoPtr->scanWidth  > 599)
      scaleWindowSize=200;

    qDebug()<<"spectrogram::setMovieImageSize"<<
              "this->imgWidth"<<this->imgWidth<<
              "this->imgHeight"<<this->imgHeight;

    //how much reduction will be done while fitting?
    if (this->imgWidth>this->imgHeight)
    {
        Ratio = this->imgWidth/this->imgHeight;
        wReductionRatio = this->imgWidth/scaleWindowSize;
        hAfterScaled = this->imgHeight/wReductionRatio;

        if (hAfterScaled < 30)
        {
            wReductionRatio = this->imgHeight/30;
            scaleWindowSize = this->imgWidth/wReductionRatio;
        }

        fontReductionFactor = 1;
        if (Ratio>2 && Ratio < 4)
            fontReductionFactor = 1.05;
        else if (Ratio>4)
            fontReductionFactor = 1.15;
    }
    else
    {
        Ratio = this->imgHeight/this->imgWidth;

        hReductionRatio = this->imgHeight/scaleWindowSize;
        wAfterScaled = this->imgWidth/hReductionRatio;

        if (wAfterScaled < 30)
        {
            hReductionRatio = this->imgWidth/30;
            scaleWindowSize = this->imgHeight/hReductionRatio;
        }

        fontReductionFactor = 1;
        if (Ratio>2 && Ratio < 4)
            fontReductionFactor = 1.05;
        else if (Ratio>4)
            fontReductionFactor = 1.15;
    }

    for (int i = 0;i<3;i++)
    {
        origFontSize[i] = this->axisFont(i).pointSizeF();
        tempFont.setPointSize(origFontSize[i]/fontReductionFactor);
        this->setAxisFont(i,tempFont);
    }

    QwtPlotRenderer renderer;
    QSizeF sizeMM(this->imgWidth,this->imgHeight);
    sizeMM.scale(scaleWindowSize, scaleWindowSize,Qt::KeepAspectRatio);

    const QSizeF size = sizeMM * mmToInch * resolution;
    const QRectF documentRect( 0.0, 0.0, size.width(), size.height() );

    imageRect = documentRect.toRect();
    const int dotsPerMeter = qRound( resolution * mmToInch * 1000.0 );

    if (imageForMovie!=NULL)
    {
        delete imageForMovie;
        imageForMovie = NULL;
    }

    imageForMovie = new QImage( imageRect.size(), QImage::Format_ARGB32 );
    imageForMovie->setDotsPerMeterX( dotsPerMeter );
    imageForMovie->setDotsPerMeterY( dotsPerMeter );
    imageForMovie->fill( QColor( Qt::white ).rgb() );

    pixMapCurptr = new QPixmap (imageRect.size());
    painterForMovie = new QPainter( pixMapCurptr );

}

QPixmap spectrogram::getPlotPixmap(int frameNum)
{
    renderer.render(this,painterForMovie,imageRect);

    if(frameNum == (SAMPLESPERPOINT-1))
    {
        QFont tempFont;
        for (int i = 0;i<3;i++)
        {
            tempFont.setPointSize(10);
            this->setAxisFont(i,tempFont);
        }
    }

    return(QPixmap::fromImage(pixMapCurptr->toImage().mirrored()));
}

void spectrogram::SaveMovie(QString outfolderpath)
{
    QString file("demo.avi");
    QByteArray fileArr = file.toUtf8();
    HBITMAP hbm;

    HAVI avi = CreateAvi(fileArr.toStdString().c_str(),100,NULL);
    QList<QPixmap> images;
    for (int frame=0; frame < images.size(); frame++) {
        hbm = qt_pixmapToWinHBITMAP(images.at(frame));
        AddAviFrame(avi,hbm);
    }
    CloseAvi(avi);
}

void spectrogram::updateData(void *framePointer, QString title, bool isIntData)
{

    if (title != NULL)
    {
        QString areaString = NULL;
        int indexOfArea = this->title.indexOf("  -  s");
        if ( indexOfArea != -1) // appending the area string for the first time
            areaString = this->title.remove(0,indexOfArea);

        this->title = title + areaString;
        this->setTitle(this->title);
    }
    else
    {
        this->setTitle(NULL);
    }

    if (this->data->isIntData            = isIntData)
        this->data->intframePointer      = (int*)framePointer;
    else
        this->data->shortframePointer    = (short*)framePointer;

    if (this->zoomer->isIntData          = isIntData)
        this->zoomer->intframePointer    = (int*)framePointer;
    else

        this->zoomer->shortframePointer  = (short*)framePointer;

    this->isIntData = isIntData;

    setcolorbar(); //isIntData means VTWAM result

    replot();
    repaint();
}

