#include <qwt_plot.h>
#include <qwt_interval.h>
#include <qwt_system_clock.h>
#include "structDef.h"

class QwtPlotCurve;
class QwtPlotMarker;
class QwtPlotDirectPainter;

class Plot: public QwtPlot
{
    Q_OBJECT

public:
    Plot(QWidget * = NULL, bool vMarkEnArg=false , QString title=NULL, structDaq *daqInfoPtrArg=NULL);
    virtual ~Plot();


public Q_SLOTS:
    void UpdateCurveOsciDouble(double *y_pt);
    void UpdateCurve(short *y_pt, QString Title=NULL);
    void updateVertMarker(int markerPos);
    void updateAxisScale();
    void initPlot();

protected:

private:
    QwtPlotMarker *d_origin,*vertMarker;
    QwtPlotCurve *d_curve;
    bool replotRequested;
    QwtPlotDirectPainter *d_directPainter;
    structDaq *daqInfoPtr;
    double* x_pt_d_loc;
    double* y_pt_d_loc;
    bool vMarkEn;
};
