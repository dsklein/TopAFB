
#include <iostream>
using std::cout;
using std::endl;

#include "TRandom3.h"
#include "TH1D.h"
#include "TUnfold.h"
#include "TMatrixD.h"

#include "src/RooUnfoldResponse.h"
#include "src/RooUnfoldBayes.h"
#include "src/RooUnfoldSvd.h"
#include "src/RooUnfoldTUnfold.h"

#include "AfbFinalUnfold.h"

#include "tdrstyle.C"
#include "../CommonFunctions.C"

//==============================================================================
// Global definitions
//==============================================================================

// 0=SVD, 1=TUnfold via RooUnfold, 2=TUnfold
int unfoldingType = 0;

TString Region = "";


Int_t kterm = 3;
Double_t tau = 1E-4;
Int_t nPseudos = 1;
Int_t includeSys = 0;

Int_t lineWidth = 5;


//TestType: "Pull" or "Linearity"
//Var2D: "mtt" or "ttrapidity2" or "ttpt"
//slopeOption: 0 = continuous reweighting, 1 = 2-binned reweighting (i.e. sign(var))
void AfbUnfoldTests2D_weightMCtoData(Int_t iVar = 0, TString TestType = "Pull", TString Var2D = "mtt", Int_t slopeOption = 0)
{
#ifdef __CINT__
    gSystem->Load("libRooUnfold");
#endif

    setTDRStyle();
    gStyle->SetOptTitle(0);
    gStyle->SetOptFit();
    gStyle->SetOptStat("emr");
    cout.precision(3);

    const Int_t n2Dvar = 3;
    TString Var2DString[n2Dvar] = {"mtt", "ttrapidity2", "ttpt"};

    ofstream myfile;
    //myfile.open ("summary_PEtest_2D.txt");
    //cout.rdbuf(myfile.rdbuf());
    ofstream second_output_file;
    //second_output_file.open("summary_PEtest_2D_formated.txt");

    TRandom3 *random = new TRandom3();
    random->SetSeed(5);

    //initialise 1D binning to get xmin and xmax for the asym variable
    Initialize1DBinning(iVar);

    Float_t asym_centre = (xmax + xmin) / 2.;

    TFile *filedataMC[n2Dvar];
    TH1D *dataMCweight[n2Dvar];

    for (int i = 0; i < n2Dvar; ++i)
    {
        //initialise 2D binning for the weighting histos
        if (Var2DString[i] == "mtt") Initialize2DBinning(iVar);
        else if (Var2DString[i] == "ttrapidity2") Initialize2DBinningttrapidity2(iVar);
        else if (Var2DString[i] == "ttpt") Initialize2DBinningttpt(iVar);
        filedataMC[i] = new TFile("DataMC_" + Var2DString[i] + ".root");
        dataMCweight[i] = (TH1D *) filedataMC[i]->Get("hDataMCratio" + Var2DString[i] + acceptanceName);
    }

    //initialise 2D binning for real
    if (Var2D == "mtt") Initialize2DBinning(iVar);
    else if (Var2D == "ttrapidity2") Initialize2DBinningttrapidity2(iVar);
    else if (Var2D == "ttpt") Initialize2DBinningttpt(iVar);


    TString file_name = "residuals_" + acceptanceName + "_" + Var2D;
    ofstream third_output_file;
    third_output_file.open(file_name + ".txt");


    const Int_t nbins2Do2 = nbins2D / 2;

    double xbins2D_positive[nbins2Do2 + 1] = {xbins2D[3], xbins2D[4], xbins2D[5], xbins2D[6]};
    if (Var2D == "mtt") xbins2D_positive[0] = 300.0;
    TH1D *hAfbVs2Dvar[n2Dvar + 1];

    for (int i = 0; i < n2Dvar + 1; ++i)
    {
        hAfbVs2Dvar[i] = new TH1D ("hAfbVs2Dvar" + i,  "hAfbVs2Dvar" + i,  nbins2Do2, xbins2D_positive);
    }


    bool combineLepMinus = acceptanceName == "lepCosTheta" ? true : false;

    TH1D *hTrue_before = new TH1D ("trueBeforeScaling", "Truth",    nbins2D, xbins2D);
    TH1D *hTrue_before_denom = new TH1D ("trueBeforeScalingacccorrected", "Truth",    nbins2D, xbins2D);
    TH1D *hMeas_before = new TH1D ("measBeforeScaling", "Measured", nbins2D, xbins2D);

    TH1D *hTrue_after = new TH1D ("trueAfterScaling", "Truth",    nbins2D, xbins2D);
    TH1D *hMeas_after = new TH1D ("measAfterScaling", "Measured", nbins2D, xbins2D);

    TH1D *hSmeared = new TH1D ("smeared", "Smeared", nbins2D, xbins2D);
    TH1D *hUnfolded = new TH1D ("unfolded", "Unfolded", nbins2D, xbins2D);

    double pullMax = 5;
    int pullBins = 50;
    if (TestType == "Linearity") pullMax = 100;
    if (TestType == "Linearity") pullBins = 1000;



    TH1D *AfbPull = new TH1D("h_afbpull", "Pulls for Afb", pullBins, -pullMax, pullMax);
    TH1D *Afb2DPullBin1 = new TH1D("h_afb2DpullBin1", "Pulls for Afb 2D Bin1", pullBins, -pullMax, pullMax);
    TH1D *Afb2DPullBin2 = new TH1D("h_afb2DpullBin2", "Pulls for Afb 2D Bin2", pullBins, -pullMax, pullMax);
    TH1D *Afb2DPullBin3 = new TH1D("h_afb2DpullBin3", "Pulls for Afb 2D Bin3", pullBins, -pullMax, pullMax);

    TH2D *hTrue_vs_Meas = new TH2D ("true_vs_meas", "True vs Measured", nbins2D, xbins2D, nbins2D, xbins2D);


    hTrue_before->Sumw2();
    hMeas_before->Sumw2();
    hTrue_after->Sumw2();
    hMeas_after->Sumw2();
    hSmeared->Sumw2();
    hUnfolded->Sumw2();

    AfbPull->Sumw2();
    Afb2DPullBin1->Sumw2();
    Afb2DPullBin2->Sumw2();
    Afb2DPullBin3->Sumw2();
    hTrue_vs_Meas->Sumw2();

    TMatrixD m_unfoldE(nbins2D, nbins2D);


    TH1F *h_pulls[nbins2D];
    TH1F *h_resd[nbins2D];
    for (int i = 0; i < nbins2D; i++)
    {
        TString name = "h_pull_";
        name += i;
        h_pulls[i] = new TH1F(name, name, 50, -5.0, 5.0);
        name = "h_resd_";
        name += i;
        h_resd[i] = new TH1F(name, name, 20, -1, 1);
    }


    TFile *file = new TFile("../ttdil.root");
    TTree *evtree = (TTree *) file->Get("tree");
    Int_t entries = (Int_t)evtree->GetEntries();
    cout << "RESPONSE: Number of Entries: " << entries << endl;

    Float_t observable, observable_gen, tmass;
    Float_t observableMinus, observableMinus_gen;
    Float_t obs2D, obs2D_gen;
    Float_t obs2Dforweighting_gen;
    Float_t mtt_gen, ttpt_gen, ttrapidity2_gen;
    Double_t weight;
    Int_t Nsolns;

    evtree->SetBranchAddress(observablename,    &observable);
    evtree->SetBranchAddress(observablename + "_gen", &observable_gen);
    if (observablename == "lep_azimuthal_asymmetry2") evtree->SetBranchAddress("lep_azimuthal_asymmetry_gen2", &observable_gen);
    if ( combineLepMinus ) evtree->SetBranchAddress("lepMinus_costheta_cms",    &observableMinus);
    if ( combineLepMinus ) evtree->SetBranchAddress("lepMinus_costheta_cms_gen",    &observableMinus_gen);
    evtree->SetBranchAddress("weight", &weight);
    evtree->SetBranchAddress("Nsolns", &Nsolns);
    evtree->SetBranchAddress("t_mass", &tmass);

    if (Var2D == "mtt")
    {
        evtree->SetBranchAddress("tt_mass", &obs2D);
        //evtree->SetBranchAddress("tt_mass_gen", &obs2D_gen);
    }
    else if (Var2D == "ttrapidity2")
    {
        evtree->SetBranchAddress("ttRapidity2", &obs2D);
        //evtree->SetBranchAddress("ttRapidity2_gen", &obs2D_gen);
    }
    else if (Var2D == "ttpt")
    {
        evtree->SetBranchAddress("ttPt", &obs2D);
        //evtree->SetBranchAddress("ttPt_gen", &obs2D_gen);
    }


    evtree->SetBranchAddress("tt_mass_gen", &mtt_gen);
    evtree->SetBranchAddress("ttRapidity2_gen", &ttrapidity2_gen);
    evtree->SetBranchAddress("ttPt_gen", &ttpt_gen);


    //Float_t slope = 0.0;
    const int Nlin = n2Dvar;
    Float_t A_gen[Nlin], Aerr_gen[Nlin], A_unf[Nlin], Aerr_unf[Nlin], A_meas[Nlin], Aerr_meas[Nlin];
    Float_t A_pull[Nlin], A_pullwidth[Nlin], Aerr_pull[Nlin], Aerr_pullwidth[Nlin];
    Float_t A_pull_bin1[Nlin], A_pullwidth_bin1[Nlin], Aerr_pull_bin1[Nlin], Aerr_pullwidth_bin1[Nlin];
    Float_t A_pull_bin2[Nlin], A_pullwidth_bin2[Nlin], Aerr_pull_bin2[Nlin], Aerr_pullwidth_bin2[Nlin];
    Float_t A_pull_bin3[Nlin], A_pullwidth_bin3[Nlin], Aerr_pull_bin3[Nlin], Aerr_pullwidth_bin3[Nlin];

    Float_t A_unf2Dbin1[Nlin], A_unf2Dbin2[Nlin], A_unf2Dbin3[Nlin];
    Float_t Aerr_unf2Dbin1[Nlin], Aerr_unf2Dbin2[Nlin], Aerr_unf2Dbin3[Nlin];
    Float_t A_gen2Dbin1[Nlin], A_gen2Dbin2[Nlin], A_gen2Dbin3[Nlin];
    Float_t Aerr_gen2Dbin1[Nlin], Aerr_gen2Dbin2[Nlin], Aerr_gen2Dbin3[Nlin];



    TH1D *hTrue_after_array[Nlin];
    TH1D *hMeas_after_array[Nlin];

    for (int k = 0; k < Nlin; k++)
    {

        if ((TestType == "Pull") && (k == 1)) break;

        //slope = -0.3 + 0.1 * k;
        //cout << "slope =" << slope << "\n";

        hTrue_before->Reset();
        hMeas_before->Reset();
        hTrue_after->Reset();
        hMeas_after->Reset();
        hTrue_vs_Meas->Reset();
        AfbPull->Reset();
        Afb2DPullBin1->Reset();
        Afb2DPullBin2->Reset();
        Afb2DPullBin3->Reset();

        for (Int_t i = 0; i < entries; i++)
        {
            evtree->GetEntry(i);

            if ( tmass > 0  )
            {
                if (Var2D == "mtt") obs2D_gen = mtt_gen;
                else if (Var2D == "ttrapidity2") obs2D_gen = ttrapidity2_gen;
                else if (Var2D == "ttpt") obs2D_gen = ttpt_gen;

                if (Var2DString[k] == "mtt") obs2Dforweighting_gen = mtt_gen;
                else if (Var2DString[k] == "ttrapidity2") obs2Dforweighting_gen = ttrapidity2_gen;
                else if (Var2DString[k] == "ttpt") obs2Dforweighting_gen = ttpt_gen;

                obs2D = fabs(obs2D);
                obs2D_gen = fabs(obs2D_gen);
                obs2Dforweighting_gen = fabs(obs2Dforweighting_gen);

                Double_t orig_weight = weight;

                Double_t variable_gen = sign(observable_gen - asym_centre) * obs2Dforweighting_gen;
                Double_t variableMinus_gen = sign(observableMinus_gen - asym_centre) * obs2Dforweighting_gen;

                //find the bin from which to take the data/MC weight
                Int_t genbin =  dataMCweight[k]->FindBin( variable_gen );
                Int_t genbinMinus =  dataMCweight[k]->FindBin( variableMinus_gen );
                if (genbin < 1) genbin = 1;
                if (genbin > nbins2D) genbin = nbins2D;
                if (genbinMinus < 1) genbinMinus = 1;
                if (genbinMinus > nbins2D) genbinMinus = nbins2D;

                Double_t dataMCw = dataMCweight[k]->GetBinContent(genbin);
                Double_t dataMCwMinus = dataMCweight[k]->GetBinContent(genbinMinus);
                //cout << dataMCw << " " << dataMCwMinus << endl;
                //if(i % 10000 == 0) cout<<i<<" "<<ch_top->GetEntries()<<endl;

                fillUnderOverFlow(hMeas_before, sign(observable - asym_centre)*obs2D, weight, Nsolns);
                fillUnderOverFlow(hTrue_before, sign(observable_gen - asym_centre)*obs2D_gen, weight, Nsolns);
                fillUnderOverFlow(hTrue_vs_Meas, sign(observable - asym_centre)*obs2D, sign(observable_gen - asym_centre)*obs2D_gen, weight, Nsolns);
                if ( combineLepMinus )
                {
                    fillUnderOverFlow(hMeas_before, sign(observableMinus - asym_centre)*obs2D, weight, Nsolns);
                    fillUnderOverFlow(hTrue_before, sign(observableMinus_gen - asym_centre)*obs2D_gen, weight, Nsolns);
                    fillUnderOverFlow(hTrue_vs_Meas, sign(observableMinus - asym_centre)*obs2D, sign(observableMinus_gen - asym_centre)*obs2D_gen, weight, Nsolns);
                }
                if (TestType == "Linearity") weight = orig_weight * dataMCw;
                fillUnderOverFlow(hMeas_after, sign(observable - asym_centre)*obs2D, weight, Nsolns);
                fillUnderOverFlow(hTrue_after, sign(observable_gen - asym_centre)*obs2D_gen, weight, Nsolns);
                if ( combineLepMinus )
                {
                    if (TestType == "Linearity") weight = orig_weight * dataMCwMinus;
                    fillUnderOverFlow(hMeas_after, sign(observableMinus - asym_centre)*obs2D, weight, Nsolns);
                    fillUnderOverFlow(hTrue_after, sign(observableMinus_gen - asym_centre)*obs2D_gen, weight, Nsolns);
                }

            }
        }

        RooUnfoldResponse response (hMeas_before, hTrue_before, hTrue_vs_Meas);

        //scale to keep total yield constant
        hMeas_after->Scale( hMeas_before->Integral() / hMeas_after->Integral() );
        hTrue_after->Scale( hTrue_before->Integral() / hTrue_after->Integral() );

        hTrue_after_array[k] = (TH1D *) hTrue_after->Clone();
        hMeas_after_array[k] = (TH1D *) hMeas_after->Clone();


        TFile *accfile = new TFile("../acceptance/mcnlo/accept_" + acceptanceName + ".root");

        TH2D *acceptM_2d = (TH2D *) accfile->Get("accept_" + acceptanceName + "_" + Var2D);

        TH1D *acceptM = new TH1D ("accept", "accept",    nbins2D, xbins2D);
        acceptM->SetBinContent(1, acceptM_2d->GetBinContent(1, 3));
        acceptM->SetBinContent(2, acceptM_2d->GetBinContent(1, 2));
        acceptM->SetBinContent(3, acceptM_2d->GetBinContent(1, 1));

        acceptM->SetBinContent(4, acceptM_2d->GetBinContent(2, 1));
        acceptM->SetBinContent(5, acceptM_2d->GetBinContent(2, 2));
        acceptM->SetBinContent(6, acceptM_2d->GetBinContent(2, 3));

        acceptM->Scale(1.0 / acceptM->Integral());


        for (Int_t accbin = 1; accbin <= nbins2D; accbin++)
        {

            if (acceptM->GetBinContent(accbin) != 0)
            {
                hTrue_after->SetBinContent(accbin, hTrue_after->GetBinContent(accbin) * 1.0 / acceptM->GetBinContent(accbin));
                hTrue_after->SetBinError  (accbin, hTrue_after->GetBinError  (accbin) * 1.0 / acceptM->GetBinContent(accbin));

            }
        }

        if (k == 0)
        {
            hTrue_before_denom = (TH1D *) hTrue_before->Clone();

            for (Int_t accbin = 1; accbin <= nbins2D; accbin++)
            {

                if (acceptM->GetBinContent(accbin) != 0)
                {
                    hTrue_before_denom->SetBinContent(accbin, hTrue_before_denom->GetBinContent(accbin) * 1.0 / acceptM->GetBinContent(accbin));
                    hTrue_before_denom->SetBinError  (accbin, hTrue_before_denom->GetBinError  (accbin) * 1.0 / acceptM->GetBinContent(accbin));

                }
            }

        }


        Float_t Afb, AfbErr;

        GetAfb(hTrue_after, Afb, AfbErr);
        A_gen[k] = Afb;
        Aerr_gen[k] = 0.0;
        cout << " True after re-weighting   : " << Afb << " +/-  " << AfbErr << "\n";

        GetAfb(hMeas_after, Afb, AfbErr);
        A_meas[k] = Afb;
        Aerr_meas[k] = AfbErr;
        cout << " Measured after re-weighting   : " << Afb << " +/-  " << AfbErr << "\n";


        vector<double> afbtrue2D;
        vector<double> afbtrue2Derr;

        if (k == 0)
        {
            GetAvsY(hTrue_before_denom, m_unfoldE, afbtrue2D, afbtrue2Derr, second_output_file);
            //true errors are much smaller (from denominator)
            afbtrue2Derr[0] = 0.0;
            afbtrue2Derr[1] = 0.0;
            afbtrue2Derr[2] = 0.0;

            for (int nb = 0; nb < nbins2Do2; nb++)
            {
                hAfbVs2Dvar[Nlin]->SetBinContent(nb + 1, afbtrue2D[nb]);
                hAfbVs2Dvar[Nlin]->SetBinError(nb + 1, afbtrue2Derr[nb]);
            }
        }

        GetAvsY(hTrue_after, m_unfoldE, afbtrue2D, afbtrue2Derr, second_output_file);
        //true errors are much smaller (from denominator)
        afbtrue2Derr[0] = 0.0;
        afbtrue2Derr[1] = 0.0;
        afbtrue2Derr[2] = 0.0;

        for (int nb = 0; nb < nbins2Do2; nb++)
        {
            hAfbVs2Dvar[k]->SetBinContent(nb + 1, afbtrue2D[nb]);
            hAfbVs2Dvar[k]->SetBinError(nb + 1, afbtrue2Derr[nb]);
        }



        // Now do the pseudos

        Float_t trialAsym = 0.0, SumAsym = 0.0, SumErrAsym = 0.0;
        Float_t SumAsymBin1 = 0.0, SumErrAsymBin1 = 0.0, SumAsymBin2 = 0.0, SumErrAsymBin2 = 0.0, SumAsymBin3 = 0.0, SumErrAsymBin3 = 0.0;
        Float_t SumTrueAsymBin1 = 0.0, SumTrueErrAsymBin1 = 0.0, SumTrueAsymBin2 = 0.0, SumTrueErrAsymBin2 = 0.0, SumTrueAsymBin3 = 0.0, SumTrueErrAsymBin3 = 0.0;


        if (nPseudos > 0)
        {

            for (int i = 0; i < nPseudos; i++)
            {

                for (int j = 1; j < hMeas_after->GetNbinsX() + 1; j++)
                {
                    double fluct;
                    if (nPseudos > 1) fluct = random->Poisson(hMeas_after->GetBinContent(j));
                    else fluct = hMeas_after->GetBinContent(j);
                    hSmeared->SetBinError(j, sqrt(fluct));
                    hSmeared->SetBinContent(j, fluct);
                }


                if (unfoldingType == 0)
                {
                    RooUnfoldSvd unfold_svd (&response, hSmeared, kterm);
                    unfold_svd.Setup(&response, hSmeared);
                    //      unfold_svd.IncludeSystematics(includeSys);
                    hUnfolded = (TH1D *) unfold_svd.Hreco();
                    m_unfoldE = unfold_svd.Ereco();
                }
                else if (unfoldingType == 1)
                {
                    RooUnfoldTUnfold unfold_rooTUnfold (&response, hSmeared, TUnfold::kRegModeCurvature);
                    unfold_rooTUnfold.Setup(&response, hSmeared);
                    unfold_rooTUnfold.FixTau(tau);
                    //  unfold_rooTUnfold.IncludeSystematics(includeSys);
                    hUnfolded = (TH1D *) unfold_rooTUnfold.Hreco();
                    m_unfoldE = unfold_rooTUnfold.Ereco();
                }
                else if (unfoldingType == 2)
                {
                    TUnfold unfold_TUnfold (hTrue_vs_Meas, TUnfold::kHistMapOutputVert, TUnfold::kRegModeCurvature);
                    //  Double_t biasScale=5.0;
                    //  unfold_TUnfold.SetBias(hTrue_before);
                    unfold_TUnfold.SetInput(hSmeared);
                    unfold_TUnfold.DoUnfold(tau);
                    unfold_TUnfold.GetOutput(hUnfolded);


                    TH2D *ematrix = unfold_TUnfold.GetEmatrix("ematrix", "error matrix", 0, 0);
                    for (Int_t cmi = 0; cmi < nbins2D; cmi++)
                    {
                        for (Int_t cmj = 0; cmj < nbins2D; cmj++)
                        {
                            m_unfoldE(cmi, cmj) = ematrix->GetBinContent(cmi + 1, cmj + 1);
                        }
                    }
                }
                else cout << "Unfolding TYPE not Specified" << "\n";


                for (int l = 0; l < nbins2D; l++)
                {
                    for (int j = 0; j < nbins2D; j++)
                    {
                        double corr = 1.0 / ( acceptM->GetBinContent(l + 1) * acceptM->GetBinContent(j + 1) );
                        //corr = corr * pow(xsection / dataIntegral,2) ;
                        m_unfoldE(l, j) = m_unfoldE(l, j) * corr;
                    }
                }


                for (Int_t accbin = 1; accbin <= nbins2D; accbin++)
                {

                    if (acceptM->GetBinContent(accbin) != 0)
                    {
                        hUnfolded->SetBinContent(accbin, hUnfolded->GetBinContent(accbin) * 1.0 / acceptM->GetBinContent(accbin));
                        hUnfolded->SetBinError  (accbin, hUnfolded->GetBinError  (accbin) * 1.0 / acceptM->GetBinContent(accbin));

                    }
                }

                GetCorrectedAfb(hUnfolded, m_unfoldE, Afb, AfbErr);

                vector<double> afb2D;
                vector<double> afb2Derr;
                GetAvsY(hUnfolded, m_unfoldE, afb2D, afb2Derr, second_output_file);


                AfbPull -> Fill( (Afb - A_gen[k])  / AfbErr );
                Afb2DPullBin1->Fill( (afb2D[0] - afbtrue2D[0])  / afb2Derr[0] );
                Afb2DPullBin2->Fill( (afb2D[1] - afbtrue2D[1])  / afb2Derr[1] );
                Afb2DPullBin3->Fill( (afb2D[2] - afbtrue2D[2])  / afb2Derr[2] );

                SumAsym + = Afb;
                SumErrAsym + = AfbErr;

                SumAsymBin1 + = afb2D[0];
                SumErrAsymBin1 + = afb2Derr[0];
                SumAsymBin2 + = afb2D[1];
                SumErrAsymBin2 + = afb2Derr[1];
                SumAsymBin3 + = afb2D[2];
                SumErrAsymBin3 + = afb2Derr[2];

                SumTrueAsymBin1 + = afbtrue2D[0];
                SumTrueErrAsymBin1 + = afbtrue2Derr[0];
                SumTrueAsymBin2 + = afbtrue2D[1];
                SumTrueErrAsymBin2 + = afbtrue2Derr[1];
                SumTrueAsymBin3 + = afbtrue2D[2];
                SumTrueErrAsymBin3 + = afbtrue2Derr[2];



                for (int j = 0; j < nbins2D; j++)
                {
                    double pull = (hUnfolded->GetBinContent(j + 1) - hTrue_after->GetBinContent(j + 1)) / hUnfolded->GetBinError(j + 1);
                    h_pulls[j]->Fill(pull);
                    double resd = (hUnfolded->GetBinContent(j + 1) - hTrue_after->GetBinContent(j + 1)) / hTrue_after->GetBinContent(j + 1);
                    h_resd[j]->Fill(resd);
                }
            }


            cout << "Average Asymmetry =" << SumAsym / nPseudos << " +/-  " << SumErrAsym / (nPseudos) << "\n";
            //A_unf[k] = SumAsym / nPseudos;
            //Aerr_unf[k] = SumErrAsym / nPseudos;

            //A_unf2Dbin1[k] = SumAsymBin1 / nPseudos;
            //Aerr_unf2Dbin1[k] = SumErrAsymBin1 / nPseudos;
            //A_unf2Dbin2[k] = SumAsymBin2 / nPseudos;
            //Aerr_unf2Dbin2[k] = SumErrAsymBin2 / nPseudos;
            //A_unf2Dbin3[k] = SumAsymBin3 / nPseudos;
            //Aerr_unf2Dbin3[k] = SumErrAsymBin3 / nPseudos;

            A_gen2Dbin1[k] = SumTrueAsymBin1 / nPseudos;
            Aerr_gen2Dbin1[k] = SumTrueErrAsymBin1 / nPseudos;
            A_gen2Dbin2[k] = SumTrueAsymBin2 / nPseudos;
            Aerr_gen2Dbin2[k] = SumTrueErrAsymBin2 / nPseudos;
            A_gen2Dbin3[k] = SumTrueAsymBin3 / nPseudos;
            Aerr_gen2Dbin3[k] = SumTrueErrAsymBin3 / nPseudos;


            A_unf[k] = SumAsym / nPseudos   -  A_gen[k];
            Aerr_unf[k] = SumErrAsym / nPseudos / sqrt(nPseudos);

            A_unf2Dbin1[k] = SumAsymBin1 / nPseudos   -  A_gen2Dbin1[k];
            Aerr_unf2Dbin1[k] = SumErrAsymBin1 / nPseudos / sqrt(nPseudos);
            A_unf2Dbin2[k] = SumAsymBin2 / nPseudos   -  A_gen2Dbin2[k];
            Aerr_unf2Dbin2[k] = SumErrAsymBin2 / nPseudos / sqrt(nPseudos);
            A_unf2Dbin3[k] = SumAsymBin3 / nPseudos   -  A_gen2Dbin3[k];
            Aerr_unf2Dbin3[k] = SumErrAsymBin3 / nPseudos / sqrt(nPseudos);

            if (nPseudos == 1)
            {
                Aerr_unf[k] = 0;
                Aerr_unf2Dbin1[k] = 0;
                Aerr_unf2Dbin2[k] = 0;
                Aerr_unf2Dbin3[k] = 0;
            }


            A_pull[k] = AfbPull->GetMean();
            Aerr_pull[k] = AfbPull->GetMeanError();
            A_pullwidth[k] = AfbPull->GetRMS();
            Aerr_pullwidth[k] = AfbPull->GetRMSError();

            A_pull_bin1[k] = Afb2DPullBin1->GetMean();
            Aerr_pull_bin1[k] = Afb2DPullBin1->GetMeanError();
            A_pullwidth_bin1[k] = Afb2DPullBin1->GetRMS();
            Aerr_pullwidth_bin1[k] = Afb2DPullBin1->GetRMSError();

            A_pull_bin2[k] = Afb2DPullBin2->GetMean();
            Aerr_pull_bin2[k] = Afb2DPullBin2->GetMeanError();
            A_pullwidth_bin2[k] = Afb2DPullBin2->GetRMS();
            Aerr_pullwidth_bin2[k] = Afb2DPullBin2->GetRMSError();

            A_pull_bin3[k] = Afb2DPullBin3->GetMean();
            Aerr_pull_bin3[k] = Afb2DPullBin3->GetMeanError();
            A_pullwidth_bin3[k] = Afb2DPullBin3->GetRMS();
            Aerr_pullwidth_bin3[k] = Afb2DPullBin3->GetRMSError();
        }

    }

    TGraphErrors *Asym2D_TrueUnf = new TGraphErrors (Nlin, A_gen, A_unf, Aerr_gen, Aerr_unf);

    TGraphErrors *Asym2D_TrueMeas = new TGraphErrors (Nlin, A_gen, A_meas, Aerr_gen, Aerr_meas);

    TGraphErrors *Asym2D_PullWidth = new TGraphErrors (Nlin, A_gen, A_pullwidth, Aerr_gen, Aerr_pullwidth);
    TGraphErrors *Asym2D_Pull = new TGraphErrors (Nlin, A_gen, A_pull, Aerr_gen, Aerr_pull);


    TGraphErrors *Asym2D_TrueUnfbin1 = new TGraphErrors (Nlin, A_gen2Dbin1, A_unf2Dbin1, Aerr_gen2Dbin1, Aerr_unf2Dbin1);
    TGraphErrors *Asym2D_TrueUnfbin2 = new TGraphErrors (Nlin, A_gen2Dbin2, A_unf2Dbin2, Aerr_gen2Dbin2, Aerr_unf2Dbin2);
    TGraphErrors *Asym2D_TrueUnfbin3 = new TGraphErrors (Nlin, A_gen2Dbin3, A_unf2Dbin3, Aerr_gen2Dbin3, Aerr_unf2Dbin3);

    TGraphErrors *Asym2D_PullWidthbin1 = new TGraphErrors (Nlin, A_gen2Dbin1, A_pullwidth_bin1, Aerr_gen2Dbin1, Aerr_pullwidth_bin1);
    TGraphErrors *Asym2D_Pullbin1 = new TGraphErrors (Nlin, A_gen2Dbin1, A_pull_bin1, Aerr_gen2Dbin1, Aerr_pull_bin1);
    TGraphErrors *Asym2D_PullWidthbin2 = new TGraphErrors (Nlin, A_gen2Dbin2, A_pullwidth_bin2, Aerr_gen2Dbin2, Aerr_pullwidth_bin2);
    TGraphErrors *Asym2D_Pullbin2 = new TGraphErrors (Nlin, A_gen2Dbin2, A_pull_bin2, Aerr_gen2Dbin2, Aerr_pull_bin2);
    TGraphErrors *Asym2D_PullWidthbin3 = new TGraphErrors (Nlin, A_gen2Dbin3, A_pullwidth_bin3, Aerr_gen2Dbin3, Aerr_pullwidth_bin3);
    TGraphErrors *Asym2D_Pullbin3 = new TGraphErrors (Nlin, A_gen2Dbin3, A_pull_bin3, Aerr_gen2Dbin3, Aerr_pull_bin3);




    if ((TestType == "Linearity"))
    {


        third_output_file << acceptanceName << " vs " << Var2D << " inclusive residuals, ";
        for (int k = 0; k < Nlin; k++)
        {
            third_output_file << " w_" << Var2DString[k] << " " << A_unf[k];
        }
        third_output_file << endl;

        third_output_file << acceptanceName << " vs " << Var2D << " bin1 residuals, ";
        for (int k = 0; k < Nlin; k++)
        {
            third_output_file << " w_" << Var2DString[k] << " " << A_unf2Dbin1[k];
        }
        third_output_file << endl;

        third_output_file << acceptanceName << " vs " << Var2D << " bin2 residuals, ";
        for (int k = 0; k < Nlin; k++)
        {
            third_output_file << " w_" << Var2DString[k] << " " << A_unf2Dbin2[k];
        }
        third_output_file << endl;

        third_output_file << acceptanceName << " vs " << Var2D << " bin3 residuals, ";
        for (int k = 0; k < Nlin; k++)
        {
            third_output_file << " w_" << Var2DString[k] << " " << A_unf2Dbin3[k];
        }
        third_output_file << endl;



        TCanvas *c_ttbar = new TCanvas("c_ttbar", "c_ttbar", 500, 500);
        c_ttbar->Divide(2, 2);
        c_ttbar->cd(1);
        Asym2D_TrueUnf->SetTitle(asymlabel);
        Asym2D_TrueUnf->SetMarkerStyle(23);
        Asym2D_TrueUnf->SetMarkerColor(kBlack);
        Asym2D_TrueUnf->SetMarkerSize(0.6);
        Asym2D_TrueUnf->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " inclusive (true)");
        Asym2D_TrueUnf->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " inclusive (residual)");
        Asym2D_TrueUnf->Draw("AP same");
        //Asym2D_TrueUnf->Fit("pol1");

        c_ttbar->cd(2);
        Asym2D_TrueUnfbin1->SetTitle(asymlabel);
        Asym2D_TrueUnfbin1->SetMarkerStyle(23);
        Asym2D_TrueUnfbin1->SetMarkerColor(kBlue);
        Asym2D_TrueUnfbin1->SetMarkerSize(0.6);
        Asym2D_TrueUnfbin1->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 1 (true)");
        Asym2D_TrueUnfbin1->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 1 (residual)");
        Asym2D_TrueUnfbin1->Draw("AP same");
        //Asym2D_TrueUnfbin1->Fit("pol1");

        c_ttbar->cd(3);
        Asym2D_TrueUnfbin2->SetTitle(asymlabel);
        Asym2D_TrueUnfbin2->SetMarkerStyle(23);
        Asym2D_TrueUnfbin2->SetMarkerColor(kBlue);
        Asym2D_TrueUnfbin2->SetMarkerSize(0.6);
        Asym2D_TrueUnfbin2->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 2 (true)");
        Asym2D_TrueUnfbin2->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 2 (residual)");
        Asym2D_TrueUnfbin2->Draw("AP same");
        //Asym2D_TrueUnfbin2->Fit("pol1");

        c_ttbar->cd(4);
        Asym2D_TrueUnfbin3->SetTitle(asymlabel);
        Asym2D_TrueUnfbin3->SetMarkerStyle(23);
        Asym2D_TrueUnfbin3->SetMarkerColor(kBlue);
        Asym2D_TrueUnfbin3->SetMarkerSize(0.6);
        Asym2D_TrueUnfbin3->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 3 (true)");
        Asym2D_TrueUnfbin3->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 3 (residual)");
        Asym2D_TrueUnfbin3->Draw("AP same");
        //Asym2D_TrueUnfbin3->Fit("pol1");

        c_ttbar->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck.pdf");
        c_ttbar->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck.C");



        TCanvas *c_Pull_lin = new TCanvas("c_Pull_lin", "c_Pull_lin", 500, 500);
        c_Pull_lin->Divide(2, 2);
        c_Pull_lin->cd(1);
        Asym2D_Pull->SetTitle(asymlabel);
        Asym2D_Pull->SetMarkerStyle(23);
        Asym2D_Pull->SetMarkerColor(kBlack);
        Asym2D_Pull->SetMarkerSize(0.6);
        Asym2D_Pull->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " inclusive (true)");
        Asym2D_Pull->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " inclusive pull");
        Asym2D_Pull->Draw("AP same");
        //Asym2D_Pull->Fit("pol1");

        c_Pull_lin->cd(2);
        Asym2D_Pullbin1->SetTitle(asymlabel);
        Asym2D_Pullbin1->SetMarkerStyle(23);
        Asym2D_Pullbin1->SetMarkerColor(kBlue);
        Asym2D_Pullbin1->SetMarkerSize(0.6);
        Asym2D_Pullbin1->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 1 (true)");
        Asym2D_Pullbin1->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 1 pull");
        Asym2D_Pullbin1->Draw("AP same");
        //Asym2D_Pullbin1->Fit("pol1");

        c_Pull_lin->cd(3);
        Asym2D_Pullbin2->SetTitle(asymlabel);
        Asym2D_Pullbin2->SetMarkerStyle(23);
        Asym2D_Pullbin2->SetMarkerColor(kBlue);
        Asym2D_Pullbin2->SetMarkerSize(0.6);
        Asym2D_Pullbin2->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 2 (true)");
        Asym2D_Pullbin2->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 2 pull");
        Asym2D_Pullbin2->Draw("AP same");
        //Asym2D_Pullbin2->Fit("pol1");

        c_Pull_lin->cd(4);
        Asym2D_Pullbin3->SetTitle(asymlabel);
        Asym2D_Pullbin3->SetMarkerStyle(23);
        Asym2D_Pullbin3->SetMarkerColor(kBlue);
        Asym2D_Pullbin3->SetMarkerSize(0.6);
        Asym2D_Pullbin3->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 3 (true)");
        Asym2D_Pullbin3->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 3 pull");
        Asym2D_Pullbin3->Draw("AP same");
        //Asym2D_Pullbin3->Fit("pol1");

        c_Pull_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_Pull.pdf");
        c_Pull_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_Pull.C");




        TCanvas *c_PullWidth_lin = new TCanvas("c_PullWidth_lin", "c_PullWidth_lin", 500, 500);
        c_PullWidth_lin->Divide(2, 2);
        c_PullWidth_lin->cd(1);
        Asym2D_PullWidth->SetTitle(asymlabel);
        Asym2D_PullWidth->SetMarkerStyle(23);
        Asym2D_PullWidth->SetMarkerColor(kBlack);
        Asym2D_PullWidth->SetMarkerSize(0.6);
        Asym2D_PullWidth->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " inclusive (true)");
        Asym2D_PullWidth->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " inclusive pull width");
        Asym2D_PullWidth->Draw("AP same");
        //Asym2D_PullWidth->Fit("pol1");

        c_PullWidth_lin->cd(2);
        Asym2D_PullWidthbin1->SetTitle(asymlabel);
        Asym2D_PullWidthbin1->SetMarkerStyle(23);
        Asym2D_PullWidthbin1->SetMarkerColor(kBlue);
        Asym2D_PullWidthbin1->SetMarkerSize(0.6);
        Asym2D_PullWidthbin1->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 1 (true)");
        Asym2D_PullWidthbin1->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 1 pull width");
        Asym2D_PullWidthbin1->Draw("AP same");
        //Asym2D_PullWidthbin1->Fit("pol1");

        c_PullWidth_lin->cd(3);
        Asym2D_PullWidthbin2->SetTitle(asymlabel);
        Asym2D_PullWidthbin2->SetMarkerStyle(23);
        Asym2D_PullWidthbin2->SetMarkerColor(kBlue);
        Asym2D_PullWidthbin2->SetMarkerSize(0.6);
        Asym2D_PullWidthbin2->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 2 (true)");
        Asym2D_PullWidthbin2->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 2 pull width");
        Asym2D_PullWidthbin2->Draw("AP same");
        //Asym2D_PullWidthbin2->Fit("pol1");

        c_PullWidth_lin->cd(4);
        Asym2D_PullWidthbin3->SetTitle(asymlabel);
        Asym2D_PullWidthbin3->SetMarkerStyle(23);
        Asym2D_PullWidthbin3->SetMarkerColor(kBlue);
        Asym2D_PullWidthbin3->SetMarkerSize(0.6);
        Asym2D_PullWidthbin3->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 3 (true)");
        Asym2D_PullWidthbin3->GetYaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 3 pull width");
        Asym2D_PullWidthbin3->Draw("AP same");
        //Asym2D_PullWidthbin3->Fit("pol1");

        c_PullWidth_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_PullWidth.pdf");
        c_PullWidth_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_PullWidth.C");


        gStyle->SetOptStat(0);
        TCanvas *c_asymdist_lin = new TCanvas("c_asymdist_lin", "c_asymdist_lin", 500, 500);
        c_asymdist_lin->Divide(2, 2);
        c_asymdist_lin->cd(1);
        hTrue_before->SetLineColor(TColor::GetColorDark(kRed));
        hTrue_before->SetLineWidth(1);
        hTrue_before->SetMinimum(0);
        hTrue_before->SetMaximum(1.3 * hTrue_before->GetMaximum());
        hTrue_before->SetFillStyle(0);
        hTrue_before->GetXaxis()->SetTitle(yaxislabel + " #times sign(" + xaxislabel + ")");
        hTrue_before->GetYaxis()->SetTitle("Number of events");
        hTrue_before->Draw("hist");
        hMeas_before->SetLineColor(TColor::GetColorDark(kBlue));
        hMeas_before->SetLineWidth(1);
        hMeas_before->SetFillStyle(0);
        hMeas_before->GetXaxis()->SetTitle(yaxislabel + " #times sign(" + xaxislabel + ")");
        hMeas_before->GetYaxis()->SetTitle("Number of events");
        hMeas_before->Draw("hist same");

        TLegend *leg1 = new TLegend(0.70, 0.76, 0.9, 0.93, NULL, "brNDC");
        leg1->SetEntrySeparation(0.1);
        leg1->SetFillColor(0);
        leg1->SetLineColor(0);
        leg1->SetBorderSize(0);
        leg1->SetFillStyle(0);
        leg1->SetTextSize(0.03);
        leg1->AddEntry(hTrue_before,    "gen",  "L");
        leg1->AddEntry(hMeas_before,    "reco", "L");
        leg1->Draw();


        for (int k = 0; k < Nlin; ++k)
        {
            TString slope_temp = Var2DString[k];
            c_asymdist_lin->cd(k + 2);
            hTrue_after_array[k]->SetLineColor(TColor::GetColorDark(kRed));
            hTrue_after_array[k]->SetLineWidth(1);
            hTrue_after_array[k]->SetMinimum(0);
            hTrue_after_array[k]->SetMaximum(1.3 * hTrue_after_array[k]->GetMaximum());
            hTrue_after_array[k]->SetFillStyle(0);
            hTrue_after_array[k]->GetXaxis()->SetTitle(yaxislabel + " #times sign(" + xaxislabel + "), " + slope_temp + " weighted");
            hTrue_after_array[k]->GetYaxis()->SetTitle("Number of events");
            hTrue_after_array[k]->Draw("hist");
            hMeas_after_array[k]->SetLineColor(TColor::GetColorDark(kBlue));
            hMeas_after_array[k]->SetLineWidth(1);
            hMeas_after_array[k]->SetFillStyle(0);
            hMeas_after_array[k]->GetXaxis()->SetTitle(yaxislabel + " #times sign(" + xaxislabel + "), " + slope_temp + " weighted");
            hMeas_after_array[k]->GetYaxis()->SetTitle("Number of events");
            hMeas_after_array[k]->Draw("hist same");

            TPaveText *pt1 = new TPaveText(0.65, 0.76, 0.90, 0.93, "brNDC");
            pt1->SetName("pt1name");
            pt1->SetBorderSize(0);
            pt1->SetFillStyle(0);

            TText *blah;

            Float_t Afb, AfbErr;
            GetAfb(hTrue_after_array[k], Afb, AfbErr);

            TString Asym1_temp = formatFloat(Afb, "%6.2f");
            Asym1_temp.ReplaceAll(" " , "" );
            Asym1_temp = TString(" Asym (gen): ") +  Asym1_temp;
            blah = pt1->AddText(Asym1_temp.Data());
            blah->SetTextSize(0.03);
            blah->SetTextAlign(11);
            blah->SetTextColor(TColor::GetColorDark(kRed));

            GetAfb(hMeas_after_array[k], Afb, AfbErr);

            TString Asym2_temp = formatFloat(Afb, "%6.2f");
            Asym2_temp.ReplaceAll(" " , "" );
            Asym2_temp = TString(" Asym (reco): ") +  Asym2_temp;
            blah = pt1->AddText(Asym2_temp.Data());
            blah->SetTextSize(0.03);
            blah->SetTextAlign(11);
            blah->SetTextColor(TColor::GetColorDark(kBlue));

            pt1->Draw();

        }

        c_asymdist_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_AsymDists.pdf");
        c_asymdist_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_AsymDists.C");









        TCanvas *c_AvsY_lin = new TCanvas("c_AvsY_lin", "c_AvsY_lin", 500, 500);
        //c_AvsY_lin->Divide(2, 2);
        TLegend *leg1 = new TLegend(0.70, 0.76, 0.9, 0.93, NULL, "brNDC");
        leg1->SetEntrySeparation(0.1);
        leg1->SetFillColor(0);
        leg1->SetLineColor(0);
        leg1->SetBorderSize(0);
        leg1->SetFillStyle(0);
        leg1->SetTextSize(0.03);

        for (int k = 0; k < Nlin + 1; ++k)
        {
            //c_AvsY_lin->cd(k + 1);
            hAfbVs2Dvar[k]->SetLineColor(Nlin + 1 - k);
            hAfbVs2Dvar[k]->SetLineWidth(1);
            hAfbVs2Dvar[k]->SetMinimum(hAfbVs2Dvar[Nlin]->GetMinimum() - 0.1);
            hAfbVs2Dvar[k]->SetMaximum(hAfbVs2Dvar[Nlin]->GetMaximum() + 0.1);
            hAfbVs2Dvar[k]->SetFillStyle(0);
            hAfbVs2Dvar[k]->GetXaxis()->SetTitle(yaxislabel);
            hAfbVs2Dvar[k]->GetYaxis()->SetTitle(asymlabel);
            if (k == 0) hAfbVs2Dvar[k]->Draw("hist");
            else hAfbVs2Dvar[k]->Draw("hist same");

            if (k < Nlin) leg1->AddEntry(hAfbVs2Dvar[k],    Var2DString[k],  "L");
            else leg1->AddEntry(hAfbVs2Dvar[k],    "default",  "L");
        }
        leg1->Draw();

        c_AvsY_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_AvsY.pdf");
        c_AvsY_lin->SaveAs(acceptanceName + "_" + Var2D + "_LinearityCheck_AvsY.C");









    }
    else
    {
        TCanvas *c_pull = new TCanvas("c_pull", "c_pull", 500, 500);
        c_pull->Divide(2, 2);
        c_pull->cd(1);
        AfbPull->SetMarkerStyle(23);
        AfbPull->SetMarkerColor(kBlack);
        AfbPull->SetMarkerSize(0.6);
        AfbPull->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " inclusive pull");
        AfbPull->GetYaxis()->SetTitle("Number of PEs / 0.2");
        AfbPull ->Fit("gaus");
        AfbPull ->Draw();

        c_pull->cd(2);
        Afb2DPullBin1->SetMarkerStyle(23);
        Afb2DPullBin1->SetMarkerColor(kBlue);
        Afb2DPullBin1->SetMarkerSize(0.6);
        Afb2DPullBin1->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 1 pull");
        Afb2DPullBin1->GetYaxis()->SetTitle("Number of PEs / 0.2");
        Afb2DPullBin1 ->Fit("gaus");
        Afb2DPullBin1 ->Draw();

        c_pull->cd(3);
        Afb2DPullBin2->SetMarkerStyle(23);
        Afb2DPullBin2->SetMarkerColor(kBlue);
        Afb2DPullBin2->SetMarkerSize(0.6);
        Afb2DPullBin2->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 2 pull");
        Afb2DPullBin2->GetYaxis()->SetTitle("Number of PEs / 0.2");
        Afb2DPullBin2 ->Fit("gaus");
        Afb2DPullBin2 ->Draw();

        c_pull->cd(4);
        Afb2DPullBin3->SetMarkerStyle(23);
        Afb2DPullBin3->SetMarkerColor(kBlue);
        Afb2DPullBin3->SetMarkerSize(0.6);
        Afb2DPullBin3->GetXaxis()->SetTitle(asymlabel + " vs. " + yaxislabel + " bin 3 pull");
        Afb2DPullBin3->GetYaxis()->SetTitle("Number of PEs / 0.2");
        Afb2DPullBin3 ->Fit("gaus");
        Afb2DPullBin3 ->Draw();

        c_pull->SaveAs(acceptanceName + "_" + Var2D + "_Pull.pdf");
        c_pull->SaveAs(acceptanceName + "_" + Var2D + "_Pull.C");



        TFile *plots = new TFile(acceptanceName + "_" + Var2D + "_plots.root", "RECREATE");
        for (int i = 0; i < nbins2D; i++)
        {
            h_pulls[i] ->Write();
            h_resd[i] ->Write();
        }
        AfbPull ->Write();
        Afb2DPullBin1 ->Write();
        Afb2DPullBin2 ->Write();
        Afb2DPullBin3 ->Write();

    }

    //myfile.close();

}

#ifndef __CINT__
int main ()
{
    AfbUnfoldLinearityTest();    // Main program when run stand-alone
    return 0;
}
#endif
