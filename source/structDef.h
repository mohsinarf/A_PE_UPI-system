#ifndef STRUCTDEF_H
#define STRUCTDEF_H

#include <QString>

struct structDaq
{
    short freqMode;
    int SamplingFreq;
    unsigned int ScanPoints;
    bool subbandDecomp;
    bool ButterWorthIIREnabled;
    int startFreqBandPass;
    int stopFreqBandPass;
    short totalNumOfScans;
    short Range; //LDV range
    unsigned short chMap; // 1-> selects channel-1 , 2-> selects channel->2, 3->selects average of channel 1&2
    int daqVoltage;
    int daqTrigDelay;
    QString settingStr;
};

struct structScan
{
    float scanHeight;
    float scanWidth;
    float scanInterval;
    QString PRF;
    QString Current;
    short scansPerInspection;
    bool useCurrentResults;
    bool enableTT;
    bool quitScan;
};

struct structResult
{
    bool filtPass1en;
    short filterType;
    short filterRadius;
    short filterItr;
    bool filtPass2en;
    short filterType2;
    short filterRadius2;
    short filterItr2;
    int vtwamStartFr[MAXVTWAMRANGES];
    int vtwamEndFr[MAXVTWAMRANGES];
    int vtwamRangeNo;
};

struct structFilterTabs
{
    double a2=0,a3=0,a4=0,a5=0,b1=0,b2=0,b3=0,b4=0,b5=0;
    int startFreqBandPassKhz;
    int stopFreqBandPassKhz;
    int SamplingFreqMhz;
} ;

struct structLms
{
    QString beamDir;
    QString SOD;
    int beamDirIndex;
    int laserStep;
    double initXabs;
    double initYabs;
    double currentXabs;
    double currentYabs;
    double currentX;
    double currentY;
    bool diplayArea;
};

struct strucLmsImage {
    short xLocus, yLocus;
    double dotDistance;
    unsigned dotFrequency;
    unsigned int ppl;
    unsigned int lpi;
};

struct filtParStruct{
    short lowPassCut;
    short hiPassCut;
    short gain;
};

struct settingsStruct{
    short ldvRange;
    short samplingFreq;
    short chNum;
    int trigDelay;
    int daqVoltage;
    filtParStruct filtPar[4];
};
#endif // STRUCTDEF_H
