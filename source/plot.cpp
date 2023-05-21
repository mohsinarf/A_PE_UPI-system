#include "plot.h"
#include <qwt_plot_grid.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_directpainter.h>
#include <qwt_curve_fitter.h>
#include <qwt_painter.h>
#include <qevent.h>
#include <QTimer.h>

Plot::Plot(QWidget *parent , bool vMarkEnArg, QString title,structDaq *daqInfoPtrArg):
    QwtPlot( parent )
{
    daqInfoPtr = daqInfoPtrArg;
    setTitle(title);
    setAutoReplot( false );
    setCanvas( new QwtPlotCanvas() );

    plotLayout()->setAlignCanvasToScales( true );

    setAxisTitle( QwtPlot::xBottom, "Time (\xC2\xB5s)" );
    setAxisScale( QwtPlot::xBottom, 0, (double)((SAMPLESPERPOINT-1))/(double)(daqInfoPtr->SamplingFreq));

    setAxisScale( QwtPlot::yLeft,-10000,10000 ); // 32752 -> 3.49 & -32768 -> -3.49

    //change the yaxis display
    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->setPen( Qt::gray, 0.0, Qt::DotLine );
    grid->enableX( true );
    grid->enableXMin( true );
    grid->enableY( true );
    grid->enableYMin( false );
    grid->attach( this );

    d_curve = new QwtPlotCurve();
    d_curve->setStyle( QwtPlotCurve::Lines );
    d_curve->setPen( canvas()->palette().color( QPalette::WindowText ) );
    d_curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
    d_curve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
    d_curve->attach( this );

    if (this->vMarkEn = vMarkEnArg)
    {
        vertMarker = new QwtPlotMarker();
        vertMarker->setLineStyle( QwtPlotMarker::VLine );
        vertMarker->setLinePen( Qt::blue, 2, Qt::SolidLine );
        vertMarker->attach( this );
        vertMarker->setVisible(false);
    }
    else
        vertMarker=NULL;

    x_pt_d_loc = new double[SAMPLESPERPOINT]();
    y_pt_d_loc = new double[SAMPLESPERPOINT]();
}

Plot::~Plot()
{
    delete x_pt_d_loc;
    delete y_pt_d_loc;
}

void Plot::updateAxisScale()
{
    setAxisTitle( QwtPlot::xBottom, "Time (\xC2\xB5s)" );
    setAxisScale( QwtPlot::xBottom, 0, (double)((SAMPLESPERPOINT))/(double)(daqInfoPtr->SamplingFreq));
    setAxisScale( QwtPlot::yLeft,-32768,32767 ); // 32752 -> 3.49 & -32768 -> -3.49
    updateAxes();

    for (int i=0;i<SAMPLESPERPOINT;i++)
        x_pt_d_loc[i] = (double)((i))/(double)(daqInfoPtr->SamplingFreq); // initiate once
}

void Plot::UpdateCurve(short *y_pt, QString Title)
{
   for (int i=0;i<SAMPLESPERPOINT;i++)
        y_pt_d_loc[i] = y_pt[i]; // just copy through pointer

   this->d_curve->setSamples(x_pt_d_loc,y_pt_d_loc,SAMPLESPERPOINT);

   if(this->vMarkEn)
       vertMarker->setVisible(true);
   if (Title!=NULL)
       setTitle(Title);

   replot();
}

void Plot::UpdateCurveOsciDouble(double *y_pt)
{
    setAxisScale( QwtPlot::yLeft,-3.5,3.5); // 32752 -> 3.49 & -32768 -> -3.49
    this->d_curve->setSamples(x_pt_d_loc,y_pt,SAMPLESPERPOINT);
    replot();
}

void Plot::initPlot()
{
    setTitle("");
    for (int i=0;i<SAMPLESPERPOINT;i++)
         y_pt_d_loc[i] = 50000; //outside of the y-axis range

    this->d_curve->setSamples(x_pt_d_loc,y_pt_d_loc,SAMPLESPERPOINT);
    replot();
}

void Plot::updateVertMarker(int markerPos)
{
    vertMarker->setValue( ((double)(double)((markerPos))/(double)(daqInfoPtr->SamplingFreq)) ,0.0);
    replot();
}



