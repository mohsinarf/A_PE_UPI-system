#include "daqControllerF.h"

daqControllerF::daqControllerF(structDaq *daqInfoPtrArg, structScan *scanInfoPtrArg)
{
    daqInfoPtr      = daqInfoPtrArg;
    scanInfoPtr     = scanInfoPtrArg;
    ConfigDone      = false;
    binaryWfmBuff   = NULL;
    OsciModeEn      = true;
    allowOsciUpdate = false;
    g_AddrSipFMC10x = 0x00;

//    updateTimerdaq = new QTimer(this);
//    connect(updateTimerdaq, SIGNAL(timeout()), this, SLOT(setallowOsciUpdate()));
}

int daqControllerF::Configure()
{
    uint32_t AddrSipFMC10x;
    uint32_t size = 1;
    int32_t nCurItemID;
    int32_t MIG_freq = 150;
    int32_t rc = 0;
    int32_t m, n, clkmult;

    QMessageBox msgb;
    int pll_ref_clkmode = 0; //0 (internal) , 1(external)
    int VCO_freq = 50;

    // convert the MHz frequency into a M and N
    if(MIG_freq>125)		clkmult =2;
    else if(MIG_freq>100 && MIG_freq<=125)	clkmult =3;
    else	clkmult =4;

    m = MIG_freq*clkmult;
    n = clkmult;

    // Open device and check handle is valid. Note that this also set up synthesizer frequency
    // This also does master reset
    if(sipif_init(2000, m, n)!=SIPIF_ERR_OK) {
        msgb.setText("Cannot initialize the hardware");
        msgb.exec();
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////////////
    // Initialize the sip CID table
    rc = (cid_init(0));
    qDebug()<<rc;

    if(rc<1) {
        msgb.setText("Could not read the CID table from the hardware");
        msgb.exec();
        return EXIT_FAILURE;
    }

    //get star offset and addresses for FMC104
    if(cid_getstaroffset(FMC104_ID, &AddrSipFMC10x, &size)!=SIP_CID_ERR_OK) {
        msgb.setText("Could not obtain address for star type");
        msgb.exec();
        sipif_free();
        return EXIT_FAILURE;
    }

    if( ConfigureDAQ(VCO_freq, nCurItemID , AddrSipFMC10x, pll_ref_clkmode) != TRUE)
    {
        msgb.setText("DAQ configuration failed");    msgb.exec();
        sipif_free();
        return EXIT_FAILURE;
    }
    msgb.setText("DAQ Configured Successfully");    msgb.exec();

    g_AddrSipFMC10x = AddrSipFMC10x;

    return EXIT_SUCCESS;
}

int daqControllerF::StartAcquisition()
{
//    // Read Star Offsets and compute sub mapping for stars
//    uint32_t dword,div_factor,sigcntr,awssigcntr;
//    // Calculate BAR of FMC10x_CTRL module in HDL
//    uint32_t AddrSipFMC10xCtrl    	= AddrSipFMC10x + 0x000;
//    uint32_t ADDR_DIV_FACTOR        = AddrSipFMC10xCtrl + 0x012;
//    uint32_t ADDR_CHANNEL_SW        = AddrSipFMC10xCtrl + 0x013;
//    uint32_t ADDR_NB_SAMPLES        = AddrSipFMC10xCtrl + 0x013;
//    uint32_t ch_sel;

//    bool scan_finish = false;
//    int start_time = 0;;

//    div_factor = 50/daqInfoPtr->SamplingFreq;

//    qDebug() <<  "div_factor = "<< div_factor ;
//    dword = 0x00;
//    //Enable test mode (for firmware DAQ_design2d_test -> output_3)
//    //test mode (0x03 = test mode, 0x00 = normal mode)
//    if( sipif_writesipreg(AddrSipFMC10x + 0x08, dword) != SIPIF_ERR_OK){
//        return -1;

//    }
//    ch_sel = 1;


//    if( sipif_writesipreg(ADDR_CHANNEL_SW, ch_sel) != SIPIF_ERR_OK){ //select channel
//        printf("Cannot select channel\n");
//        return -1;
//    }

//    if( sipif_writesipreg(ADDR_DIV_FACTOR, div_factor) != SIPIF_ERR_OK){ //divide factor 25 and used f0r 2MHZ clk
//        printf("Cannot write divide factor\n");
//        return -1;
//    }


//        // compute the ADC channel enabled
//        unsigned char adc0en = ENABLED;

//        //One time enabling is and Arming enough
//        if(FMC10x_ctrl_enable_channel(AddrSipFMC10xCtrl, adc0en)!=FMC10x_CTRL_ERR_OK) {
//            sipif_free();
//            return -21;
//        }

//        // arm the DAC
//        if(FMC10x_ctrl_arm_dac(AddrSipFMC10xCtrl)!=FMC10x_CTRL_ERR_OK) {
//            sipif_free();
//            return -22;
//        }

//        //Enable channel 0 of ADC and External trigger rising edge
//        if (FMC10x_cntrl_ext_trigNen_ch(AddrSipFMC10xCtrl,1) != FMC10x_CTRL_ERR_OK){
//            sipif_free();
//            return -21;
//        }

//        start_time = clock();

////      /////////////////////////////////////
////      //waiting for scanning to finish//

//        do {
//        //checking regsiter 'scan_finish', bit '11' for ddr_tx_complete
//        sipif_readsipreg (Addr_uwpi_regs, &dword);
//        //checking thru bit masking
//        qDebug() <<  "scan finish first = " <<dword ;
//        scan_finish =  (dword  == 3 || dword  == 1 ) ? true : false;

//        if(scan_finish != false)

//        qDebug() <<  "scan finish if true = " <<dword ;
//        else
//           sipif_readsipreg(Addr_inp_sigcntr, &sigcntr);
//           sipif_readsipreg(Addr_inp_awssigcntr, &awssigcntr);
//           ui->scan_progress->setValue(int (sigcntr));
//           QCoreApplication::processEvents();
//              qDebug() <<  "uwpi signal if false" <<sigcntr ;
//              qDebug() <<  "aws signal if false" <<awssigcntr ;
//          } while(scan_finish == false);
////        ///////////////////////////////////


//    //Disable channel 0 of ADC and External trigger rising edge
//    if (FMC10x_cntrl_ext_trigNen_ch(AddrSipFMC10xCtrl,0) != FMC10x_CTRL_ERR_OK){
//            sipif_free();
//            return -21;
//        }

//     sipif_readsipreg(Addr_inp_sigcntr, &dword);
//     ui->scan_progress->setValue(ui->scan_progress->maximum());
//     qDebug() << endl<<"number of signals captured"<< dword<< endl;
//     qDebug() <<  "ch sel = "<< ch_sel ;

//     *adc_wr_time = (clock() - start_time)/double(CLOCKS_PER_SEC);
     return 0;
}
