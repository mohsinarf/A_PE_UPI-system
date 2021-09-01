#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include <qwt_plot.h>
#include "qwt_plot_zoomer.h"
#include <qwt_plot_spectrogram.h>
#include <qwt_plot_curve.h>
#include <qwt_color_map.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_draw.h>
#include <qwt_plot_zoomer.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_renderer.h>
#include <qwt_picker_machine.h>
#include <qwt_scale_engine.h>
#include "structDef.h"
#include <QElapsedTimer>
#include <qwt_text_label.h>
#include <QMessageBox>
//#include "aviUtil.h"


class ColorMapGrey: public QwtLinearColorMap
{
public:
    ColorMapGrey():
        QwtLinearColorMap( Qt::white, Qt::black )
    {
        //addColorStop( 0.8, Qt::white );
    }
};

class ColorMap: public QwtLinearColorMap
{
public:
    ColorMap():
        QwtLinearColorMap( Qt::blue, Qt::red )
    {
        addColorStop( 0.8, Qt::white );
    }
};

class VTWAMColorMap: public QwtLinearColorMap
{
public:
    VTWAMColorMap():
        QwtLinearColorMap(QColor(0,0,189), QColor(132,0,0))
    {
       addColorStop(1.0/12.0*1.0, QColor(0,0,255));
       addColorStop(1.0/12.0*2.0, QColor(0,66,255));
       addColorStop(1.0/12.0*3.0, QColor(0,132,255));
       addColorStop(1.0/12.0*4.0, QColor(0,189,255));
       addColorStop(1.0/12.0*5.0, QColor(0,255,255));
       addColorStop(1.0/12.0*6.0, QColor(66,255,189));
       addColorStop(1.0/12.0*7.0, QColor(132,255,132));
       addColorStop(1.0/12.0*8.0, QColor(189,255,66));
       addColorStop(1.0/12.0*9.0, QColor(255,255,0));
       addColorStop(1.0/12.0*10.0, QColor(255,189,0));
       addColorStop(1.0/12.0*11.0, QColor(255,66,0));
       addColorStop(1.0/12.0*12.0, QColor(189,0,0));
    }
};

class SpectZoomer: public QwtPlotZoomer
{
public:
    float scanInterval;
    short *shortframePointer; // just to display the current value for debugging
    int *intframePointer; // just to display the current value for debugging
    short x_length;
    bool isIntData;
    SpectZoomer( QWidget *canvas ):
        QwtPlotZoomer( canvas )
    {
        setTrackerMode( AlwaysOn );
        scanInterval = 0.5;
        x_length = 400;
        shortframePointer = NULL;
        intframePointer = NULL;
        isIntData = 0;
    }

    virtual QwtText trackerTextF( const QPointF &pos ) const
    {
        QColor bg( Qt::white );
        bg.setAlpha( 200 );
        QwtText text;
        if ( (isIntData == 0 &&shortframePointer!=NULL ) || (isIntData == 1 &&intframePointer!=NULL ))
        {
            if (pos.x() >= 0 && pos.y() >= 0)
            {
                if (isIntData)
                    text = QString::number(intframePointer[((int)(pos.x())+(int)(pos.y()*x_length))]);
                    else
                    text = QString::number(shortframePointer[((int)(pos.x())+(int)(pos.y()*x_length))]);
            }
        }

        text.setBackgroundBrush( QBrush( bg ) );
        return text;
    }

};

class GraphZoomer: public QwtPlotZoomer
{
public:
    GraphZoomer( QWidget *canvas ):
        QwtPlotZoomer( canvas )
    {
        setTrackerMode( AlwaysOn );
    }

    virtual QwtText trackerText( const QPoint &pos ) const
    {
        QColor bg( Qt::white );
        bg.setAlpha( 200 );
        QwtText text;

        //We have only one curve in the canvas so no need to iterate the whole list.
        QwtPlotCurve *Curve =   static_cast<QwtPlotCurve *> (*this->plot()->itemList().begin());
        if (Curve != NULL)
        {
            double d;
            int index = Curve->closestPoint(pos, &d);
            if (d < 5)
            {
               qDebug()<<Curve->sample(index);
               text = QwtPlotZoomer::trackerTextF(Curve->sample(index));
            }
        }

        text.setBackgroundBrush( QBrush( bg ) );

        return text;
    }
};

class mydata: public QwtRasterData
{
public:
    //keep em public for ease of access. only access from with spectrogram plot.
    int intensity;
    int x_length;
    int y_length;
    short *shortframePointer;
    int *intframePointer;
    QElapsedTimer timer;
    bool isIntData;

    //void updateDataAxis();
    mydata(int intensity = 0, int y_length = 50, int x_length = 50)
    {
        this->intensity     = intensity;
        this->x_length      = x_length;
        this->y_length      = y_length;
        this->shortframePointer  = NULL;
        this->intframePointer  = NULL;
        isIntData           = 0;
        updateDataAxis();
    }
    ~mydata()
    {

    }
    void updateDataAxis()
    {
        setInterval( Qt::XAxis, QwtInterval( 0, (x_length-0.01) ) );
        setInterval( Qt::YAxis, QwtInterval( 0, (y_length-0.01) ) );
        if (isIntData)
            setInterval( Qt::ZAxis, QwtInterval( 0 , intensity )) ;
        else
            setInterval( Qt::ZAxis, QwtInterval( -intensity, intensity )) ;
    }

    virtual double value( double x, double y ) const//shifted
    {
        int x_pos = static_cast<int>(x);
        int y_pos = static_cast<int>(y);

        if ( (isIntData == 0 && shortframePointer!=NULL ) || (isIntData == 1 &&intframePointer!=NULL ))
        {
            if (isIntData)
                return((double)(intframePointer[((x_pos)+(y_pos*x_length))])); // change a straight frame to 2D frame
                else
                return((double)(shortframePointer[((x_pos)+(y_pos*x_length))])); // change a straight frame to 2D frame

        }
        else
        { // just make a pattern in case the data pointer in NULL
            if ( ((int)x%2) == 0 || ((int)y%2) == 0 )
                return(-10000);
            else
                return(10000);
        }
    }
};

class MyScaleDraw: public QwtScaleDraw
{
public:
    float scanInterval;
    MyScaleDraw()
    {
        setLabelRotation( 0 );
        setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );
        setSpacing( 10 );

    }

    virtual QwtText label( double value ) const
    {
        QwtText h=QwtText(QString::number(value*scanInterval));
        return h;
    }

    void invalidateCacheLoc()
    {
        invalidateCache();
    }
};

class spectrogram: public QwtPlot
{
    Q_OBJECT
    int fnum;
    int dial;
    int g_x;
    int g_y;
    float g_int;
    int stop;
    int movie_type;

public:
    spectrogram(QWidget * = NULL, structScan * scanInfoPtr = NULL, short leftPosArg=0, short topPosArg=0,
                 short widthArg=SPECTBASESIZE+150, short heightArg=SPECTBASESIZE, QString titleArg=NULL ,
                int noOfSlavesArg=1);
    ~spectrogram();
    bool enlargeEnabled;

public Q_SLOTS:
    void selectPoint(QPointF Pos);
    void updateData(void *framePointer, QString title=NULL, bool isIntData=0);
    void updateAxisXY(int enlargeFactor = 0);
    void setIntensity( int );
    void savePlot(QString outfolderpath = 0);
    void SaveMovie(QString outfolderpath = 0);
    QPixmap getPlotPixmap(int frameNum);
    void setMovieImageSize();
    void placeSlaveslot(int leftPos, int topPos);
    void toggleUWPIGreyScale(bool isGreyForUWPI);
    float polyArea();
    void selectPointForPolygon( QPointF Pos );
    void clearPolyGon( QPointF Pos );


private:
    QwtPlotSpectrogram *d_spectrogram;
    SpectZoomer* zoomer;
    MyScaleDraw *scaleDrawXaxis, *scaleDrawYaxis;

    structScan *scanInfoPtr;
    short framePointer;
    mydata *data;
    short leftPos,topPos,width,height;
    QString title;
    QImage *imageForMovie;
    QPainter *painterForMovie;
    QRect imageRect;
    QwtPlotRenderer renderer;
    QPixmap *pixMapCurptr;
    QWidget *container;
    int noOfSlaves;
    bool isIntData;
    bool isGreyForUWPI;
    int imgHeight,imgWidth;
    QPolygonF pointList;
    QwtPlotCurve *boundaryCurve;


    void showSpectrogram( bool on );
    //void setAlpha( int );

    void setcolorbar();
    void go_annimation();
    void stopper( int stopper );

    signals:
    void pointToPlot(int SelectedImpingePoint);
    void placeSlave(int leftPos,int topPos);

};
#endif
