// $Id: runMonoPhoton.C,v 1.31 2013/07/31 22:45:21 dimatteo Exp $
#if !defined(__CINT__) || defined(__MAKECINT__)
#include <TSystem.h>
#include <TProfile.h>
#include "MitAna/DataUtil/interface/Debug.h"
#include "MitAna/Catalog/interface/Catalog.h"
#include "MitAna/TreeMod/interface/Analysis.h"
#include "MitAna/TreeMod/interface/HLTMod.h"
#include "MitAna/PhysicsMod/interface/RunLumiSelectionMod.h"
#include "MitAna/PhysicsMod/interface/MCProcessSelectionMod.h"
#include "MitAna/PhysicsMod/interface/PublisherMod.h"
#include "MitAna/DataTree/interface/JetCol.h"
#include "MitAna/DataTree/interface/PFJetCol.h"
#include "MitPhysics/Init/interface/ModNames.h"
#include "MitAna/DataTree/interface/Names.h"
#include "MitPhysics/Mods/interface/GeneratorMod.h"
#include "MitPhysics/Mods/interface/GoodPVFilterMod.h"
#include "MitPhysics/Mods/interface/MuonIDMod.h"
#include "MitPhysics/Mods/interface/ElectronIDMod.h"
#include "MitPhysics/Mods/interface/ElectronCleaningMod.h"
#include "MitPhysics/Mods/interface/PhotonIDMod.h"
#include "MitPhysics/Mods/interface/PhotonTreeWriter.h"
#include "MitPhysics/Mods/interface/PhotonCleaningMod.h"
#include "MitPhysics/Mods/interface/MergeLeptonsMod.h"
#include "MitPhysics/Mods/interface/JetCorrectionMod.h"
#include "MitPhysics/Mods/interface/PhotonMvaMod.h"
#include "MitPhysics/Mods/interface/MVASystematicsMod.h"
#include "MitPhysics/Mods/interface/SeparatePileUpMod.h"
#include "MitPhysics/Mods/interface/JetIDMod.h"
#include "MitPhysics/Mods/interface/JetCleaningMod.h"

#include "MitMonoPhoton/SelMods/interface/MonoPhotonAnalysisMod.h"
#include "MitMonoPhoton/Mods/interface/MonoPhotonTreeWriter.h"

#endif

//--------------------------------------------------------------------------------------------------
void runMonoPhotonSync(const char *fileset    = "0000",
		   const char *skim       = "noskim",
		   const char *dataset    = "s12-wglg-v7a", 
		   const char *book       = "t2mit/filefi/029",
		   const char *catalogDir = "/home/cmsprod/catalog",
		   const char *outputName = "gmet",
		   int         nEvents    = 1000)
{
  //------------------------------------------------------------------------------------------------
  // some parameters get passed through the environment
  //------------------------------------------------------------------------------------------------
  char json[1024], overlap[1024];
  float overlapCut = -1;
  
  if (gSystem->Getenv("MIT_PROD_JSON"))
    sprintf(json,   "%s",gSystem->Getenv("MIT_PROD_JSON"));
  else {
    sprintf(json, "%s", "~");
    //printf(" JSON file was not properly defined. EXIT!\n");
    //return;
  } 
  
  TString jsonFile = TString("/home/cmsprod/cms/json/") + TString(json);
  //TString jsonFile = TString("/home/cmsprod/cms/json/") + TString("Cert_136033-149442_7TeV_Dec22ReReco_Collisions10_JSON_v4.txt");
  Bool_t  isData   = ( (jsonFile.CompareTo("/home/cmsprod/cms/json/~") != 0) );
  
  if (gSystem->Getenv("MIT_PROD_OVERLAP")) {
    sprintf(overlap,"%s",gSystem->Getenv("MIT_PROD_OVERLAP"));
    if (EOF == sscanf(overlap,"%f",&overlapCut)) {
      printf(" Overlap was not properly defined. EXIT!\n");
      return;
    }
  }
  else {
     sprintf(overlap,"%s", "-1.0");
    //printf(" OVERLAP file was not properly defined. EXIT!\n");
    //return;
  } 

  printf("\n Initialization worked \n");

  //------------------------------------------------------------------------------------------------
  // some global setups
  //------------------------------------------------------------------------------------------------
  using namespace mithep;
  gDebugMask  = Debug::kGeneral;
  gDebugLevel = 3;

  //------------------------------------------------------------------------------------------------
  // set up information
  //------------------------------------------------------------------------------------------------
  MCProcessSelectionMod *mcselmod = new MCProcessSelectionMod;
  
  MVASystematicsMod *sysMod = new MVASystematicsMod;
  sysMod->SetMCR9Scale(1.0035, 1.0035);  
  sysMod->SetIsData(isData);
  
  RunLumiSelectionMod *runLumiSel = new RunLumiSelectionMod;      
  runLumiSel->SetAcceptMC(!isData);
  // only select on run- and lumisection numbers when valid json file present
  if ((jsonFile.CompareTo("/home/cmsprod/cms/json/~") != 0) &&
      (jsonFile.CompareTo("/home/cmsprod/cms/json/-") != 0)) {
    runLumiSel->AddJSONFile(jsonFile.Data());
  }
  if ((jsonFile.CompareTo("/home/cmsprod/cms/json/-") == 0)) {
    printf("\n WARNING -- Looking at data without JSON file: always accept.\n\n");
    runLumiSel->SetAbortIfNotAccepted(kFALSE);   // accept all events if there is no valid JSON file
  }

  printf("\n Run lumi worked\n");

  // Dataset Flags
  bool isDataSinglePh     = false;
  bool isDataPhHad        = false;
  if (TString(dataset).Contains("-pho-") || TString(dataset).Contains("-sph-")) isDataSinglePh = true;
  if (TString(dataset).Contains("-phh-")) isDataPhHad = true;

  // Generator info
  GeneratorMod *generatorMod = new GeneratorMod;
  generatorMod->SetPrintDebug(kFALSE);
  generatorMod->SetPtLeptonMin(0.0);
  generatorMod->SetEtaLeptonMax(2.7);
  generatorMod->SetPtPhotonMin(15.0);
  generatorMod->SetEtaPhotonMax(2.7);
  generatorMod->SetPtRadPhotonMin(10.0);
  generatorMod->SetEtaRadPhotonMax(2.7);
  generatorMod->SetIsData(isData);
  generatorMod->SetFillHist(!isData);
  generatorMod->SetApplyISRFilter(kFALSE);
  generatorMod->SetApplyVVFilter(kFALSE);
  generatorMod->SetApplyVGFilter(kFALSE);
  generatorMod->SetFilterBTEvents(kFALSE);

  //------------------------------------------------------------------------------------------------
  // HLT information
  //------------------------------------------------------------------------------------------------
  HLTMod *hltModP = new HLTMod("HLTModP");
  
  //------------------------------------------------------------------------------------------------
  // Run2011
  //------------------------------------------------------------------------------------------------
  hltModP->AddTrigger("HLT_Photon75_CaloIdVL_v*",160404,165087); 
  hltModP->AddTrigger("HLT_Photon125_v*",165088,163869); 
  hltModP->AddTrigger("HLT_Photon135_v*",167039,190455); 

  //------------------------------------------------------------------------------------------------
  // Run2012
  //------------------------------------------------------------------------------------------------
  if (isData){//DATA
    if (isDataSinglePh){
      hltModP->AddTrigger("HLT_Photon135_v*",190456,193686); 
      hltModP->AddTrigger("HLT_Photon150_v*",193687,199695); 
      hltModP->AddTrigger("HLT_Photon135_v*",199696,209151); 
    }
    else if (isDataPhHad){
      hltModP->AddTrigger("!HLT_Photon135_v*&HLT_Photon70_CaloIdXL_PFMET100_v*",190456,193686); 
      hltModP->AddTrigger("!HLT_Photon150_v*&HLT_Photon70_CaloIdXL_PFMET100_v*",193687,199695); 
      hltModP->AddTrigger("!HLT_Photon135_v*&HLT_Photon70_CaloIdXL_PFMET100_v*",199696,209151); 
    }
    else{
      printf("ERROR, WRONG DATASET IDENTIFICATION FOR: %s\n",dataset);
      return;
    }
  }
  else{//MC
    hltModP->AddTrigger("HLT_Photon135_v*",0,999999); 
    hltModP->AddTrigger("!HLT_Photon135_v*",0,999999); 
    hltModP->AddTrigger("HLT_Photon70_CaloIdXL_PFMET100*",0,999999); 
    hltModP->AddTrigger("!HLT_Photon70_CaloIdXL_PFMET100*",0,999999); 
  }
    
  hltModP->SetTrigObjsName("MyHltPhotObjs");
  hltModP->SetAbortIfNotAccepted(isData);
  hltModP->SetPrintTable(kTRUE); // set to true to print HLT table

  //------------------------------------------------------------------------------------------------
  // split pfcandidates to PFPU and PFnoPU
  //------------------------------------------------------------------------------------------------
  SeparatePileUpMod* SepPUMod = new SeparatePileUpMod;
  //  SepPUMod->SetUseAllVerteces(kFALSE);
  // SepPUMod->SetVertexName("OutVtxCiC");
  SepPUMod->SetPFNoPileUpName("pfnopileupcands");
  SepPUMod->SetPFPileUpName("pfpileupcands");
  SepPUMod->SetCheckClosestZVertex(kFALSE);
  
  //------------------------------------------------------------------------------------------------
  // select events with a good primary vertex
  //------------------------------------------------------------------------------------------------
  GoodPVFilterMod *goodPVFilterMod = new GoodPVFilterMod;
  goodPVFilterMod->SetMinVertexNTracks(0);
  goodPVFilterMod->SetMinNDof         (4.0);
  goodPVFilterMod->SetMaxAbsZ         (24.0);
  goodPVFilterMod->SetMaxRho          (2.0);
  goodPVFilterMod->SetIsMC(!isData);
  goodPVFilterMod->SetVertexesName("PrimaryVertexes");
  
  //------------------------------------------------------------------------------------------------
  // object id and cleaning sequence
  //------------------------------------------------------------------------------------------------
  //-----------------------------------
  // Lepton Selection 
  //-----------------------------------
  ElectronIDMod* eleIdMod = new ElectronIDMod;
  eleIdMod->SetIDType("VBTFWorkingPointLowPtId");
  eleIdMod->SetIsoType("PFIso");
  eleIdMod->SetApplyConversionFilterType1(kTRUE);
  eleIdMod->SetApplyConversionFilterType2(kFALSE);
  eleIdMod->SetChargeFilter(kFALSE);
  eleIdMod->SetApplyD0Cut(kTRUE);
  eleIdMod->SetApplyDZCut(kTRUE);
  eleIdMod->SetWhichVertex(-1);
  eleIdMod->SetNExpectedHitsInnerCut(0);
  eleIdMod->SetGoodElectronsName("GoodElectronsBS");
  eleIdMod->SetRhoType(RhoUtilities::CMS_RHO_RHOKT6PFJETS);
   
  MuonIDMod* muonIdMod = new MuonIDMod;
  // base kinematics
  muonIdMod -> SetPtMin(10.);
  muonIdMod -> SetEtaCut(2.4);
  // base ID
  muonIdMod -> SetIDType("Tight");
  muonIdMod -> SetWhichVertex(-1); // this is a 'hack'.. but hopefully good enough...
  muonIdMod -> SetD0Cut(0.02);
  muonIdMod -> SetDZCut(0.5);
  muonIdMod -> SetIsoType("PFIsoBetaPUCorrected"); //h
  muonIdMod -> SetPFIsoCut(0.2); //h
  muonIdMod -> SetOutputName("HggLeptonTagMuons");
  muonIdMod -> SetPFNoPileUpName("pfnopileupcands");
  muonIdMod -> SetPFPileUpName("pfpileupcands");
  muonIdMod -> SetPVName(Names::gkPVBeamSpotBrn); 
  
  ElectronCleaningMod *electronCleaning = new ElectronCleaningMod;
  electronCleaning->SetCleanMuonsName(muonIdMod->GetOutputName());
  electronCleaning->SetGoodElectronsName(eleIdMod->GetOutputName());
  electronCleaning->SetCleanElectronsName("CleanElectrons");

  MergeLeptonsMod *merger = new MergeLeptonsMod;
  merger->SetMuonsName(muonIdMod->GetOutputName());
  merger->SetElectronsName(electronCleaning->GetOutputName());
  merger->SetMergedName("MergedLeptons");

  //-----------------------------------
  // Photon Regression + ID 
  //-----------------------------------
  PhotonMvaMod *photreg = new PhotonMvaMod;
  photreg->SetRegressionVersion(3);
  photreg->SetRegressionWeights(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/gbrv3ph_52x.root")).Data()));
  photreg->SetOutputName("GoodPhotonsRegr");
  photreg->SetApplyShowerRescaling(kTRUE);
  photreg->SetMinNumPhotons(1);
  photreg->SetIsData(isData);
  photreg->SetDoPreselection(kFALSE);

  PhotonIDMod *photonIDMod = new PhotonIDMod;
  photonIDMod->SetPtMin(20.0);
  photonIDMod->SetOutputName("GoodPhotons");
  photonIDMod->SetIDType("BaseLineCiCPFNoPresel");
  photonIDMod->SetIsoType("NoIso");
  photonIDMod->SetApplyElectronVeto(kFALSE);
  photonIDMod->SetApplyPixelSeed(kFALSE);
  photonIDMod->SetApplyConversionId(kTRUE);
  photonIDMod->SetApplyFiduciality(kTRUE);       
  photonIDMod->SetIsData(isData);
  photonIDMod->SetPhotonsFromBranch(kFALSE);
  photonIDMod->SetInputName(photreg->GetOutputName());
  //get the photon with regression energy  
  photonIDMod->DoMCSmear(kTRUE);
  photonIDMod->DoDataEneCorr(kTRUE);
  //------------------------------------------Energy smear and scale--------------------------------------------------------------
  photonIDMod->SetMCSmearFactors2012HCP(0.0111,0.0111,0.0107,0.0107,0.0155,0.0194,0.0295,0.0276,0.037,0.0371);
  photonIDMod->AddEnCorrPerRun2012HCP(190645,190781,0.9964,0.9964,1.0020,1.0020,0.9893,1.0028,0.9871,0.9937,0.9839,0.9958);
  photonIDMod->AddEnCorrPerRun2012HCP(190782,191042,1.0024,1.0024,1.0079,1.0079,0.9923,1.0058,0.9911,0.9977,0.9886,1.0005);
  photonIDMod->AddEnCorrPerRun2012HCP(191043,193555,0.9935,0.9935,0.9991,0.9991,0.9861,0.9997,0.9894,0.9960,0.9864,0.9982);
  photonIDMod->AddEnCorrPerRun2012HCP(193556,194150,0.9920,0.9920,0.9976,0.9976,0.9814,0.9951,0.9896,0.9962,0.9872,0.9990);
  photonIDMod->AddEnCorrPerRun2012HCP(194151,194532,0.9925,0.9925,0.9981,0.9981,0.9826,0.9963,0.9914,0.9980,0.9874,0.9993);
  photonIDMod->AddEnCorrPerRun2012HCP(194533,195113,0.9927,0.9927,0.9983,0.9983,0.9844,0.9981,0.9934,0.9999,0.9878,0.9996);
  photonIDMod->AddEnCorrPerRun2012HCP(195114,195915,0.9929,0.9929,0.9984,0.9984,0.9838,0.9974,0.9942,1.0007,0.9878,0.9997);
  photonIDMod->AddEnCorrPerRun2012HCP(195916,198115,0.9919,0.9919,0.9975,0.9975,0.9827,0.9964,0.9952,1.0017,0.9869,0.9987);
  photonIDMod->AddEnCorrPerRun2012HCP(198116,199803,0.9955,0.9955,1.0011,1.0011,0.9859,0.9995,0.9893,0.9959,0.9923,1.0041);
  photonIDMod->AddEnCorrPerRun2012HCP(199804,200048,0.9967,0.9967,1.0023,1.0023,0.9870,1.0006,0.9893,0.9959,0.9937,1.0055);
  photonIDMod->AddEnCorrPerRun2012HCP(200049,200151,0.9980,0.9980,1.0036,1.0036,0.9877,1.0012,0.9910,0.9976,0.9980,1.0097);
  photonIDMod->AddEnCorrPerRun2012HCP(200152,200490,0.9958,0.9958,1.0013,1.0013,0.9868,1.0004,0.9922,0.9988,0.9948,1.0065);
  photonIDMod->AddEnCorrPerRun2012HCP(200491,200531,0.9979,0.9979,1.0035,1.0035,0.9876,1.0012,0.9915,0.9981,0.9979,1.0096);
  photonIDMod->AddEnCorrPerRun2012HCP(200532,201656,0.9961,0.9961,1.0017,1.0017,0.9860,0.9996,0.9904,0.9970,0.9945,1.0063);
  photonIDMod->AddEnCorrPerRun2012HCP(201657,202305,0.9969,0.9969,1.0025,1.0025,0.9866,1.0002,0.9914,0.9980,0.9999,1.0116);
  photonIDMod->AddEnCorrPerRun2012HCP(202305,203002,0.9982,0.9982,1.0038,1.0038,0.9872,1.0008,0.9934,1.0000,1.0018,1.0135);
  photonIDMod->AddEnCorrPerRun2012HCP(203003,203984,1.0006,1.0006,1.0061,1.0061,0.9880,1.0017,0.9919,0.9988,0.9992,1.0104);     
  photonIDMod->AddEnCorrPerRun2012HCP(203985,205085,0.9993,0.9993,1.0048,1.0048,0.9903,1.0040,0.9928,0.9997,0.9987,1.0099);     
  photonIDMod->AddEnCorrPerRun2012HCP(205086,205310,1.0004,1.0004,1.0059,1.0059,0.9901,1.0037,0.9987,1.0055,1.0091,1.0202);     
  photonIDMod->AddEnCorrPerRun2012HCP(205311,206207,1.0000,1.0000,1.0055,1.0055,0.9891,1.0028,0.9948,1.0017,1.0032,1.0144);     
  photonIDMod->AddEnCorrPerRun2012HCP(206208,206483,1.0003,1.0003,1.0058,1.0058,0.9895,1.0032,0.9921,0.9989,1.0056,1.0167);     
  photonIDMod->AddEnCorrPerRun2012HCP(206484,206597,1.0005,1.0005,1.0060,1.0060,0.9895,1.0032,0.9968,1.0036,1.0046,1.0158);     
  photonIDMod->AddEnCorrPerRun2012HCP(206598,206896,1.0006,1.0006,1.0061,1.0061,0.9881,1.0017,0.9913,0.9982,1.0050,1.0162);     
  photonIDMod->AddEnCorrPerRun2012HCP(206897,207220,1.0006,1.0006,1.0061,1.0061,0.9884,1.0021,0.9909,0.9978,1.0053,1.0165);     
  photonIDMod->AddEnCorrPerRun2012HCP(207221,208686,1.0006,1.0006,1.0061,1.0061,0.9894,1.0030,0.9951,1.0020,1.0060,1.0172);     
  //---------------------------------shower shape scale--------------------------------------------------------------------------------
  photonIDMod->SetDoShowerShapeScaling(kTRUE);
  photonIDMod->SetShowerShapeType("2012ShowerShape");
  photonIDMod->Set2012HCP(kTRUE);

  PhotonCleaningMod *photonCleaningMod = new PhotonCleaningMod;
  photonCleaningMod->SetCleanElectronsName(electronCleaning->GetOutputName());
  photonCleaningMod->SetGoodPhotonsName(photonIDMod->GetOutputName());
  photonCleaningMod->SetCleanPhotonsName("CleanPhotons");

  PublisherMod<PFJet,Jet> *pubJet = new PublisherMod<PFJet,Jet>("JetPub");
  pubJet->SetInputName("AKt5PFJets");
  pubJet->SetOutputName("PubAKt5PFJets");

  JetCorrectionMod *jetCorr = new JetCorrectionMod;
  if(isData){ 
    jetCorr->AddCorrectionFromFile(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/Summer12_V7_DATA_L1FastJet_AK5PF.txt")).Data())); 
    jetCorr->AddCorrectionFromFile(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/Summer12_V7_DATA_L2Relative_AK5PF.txt")).Data())); 
    jetCorr->AddCorrectionFromFile(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/Summer12_V7_DATA_L3Absolute_AK5PF.txt")).Data())); 
    jetCorr->AddCorrectionFromFile(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/Summer12_V7_DATA_L2L3Residual_AK5PF.txt")).Data())); 
  }
  else {
    jetCorr->AddCorrectionFromFile(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/Summer12_V7_MC_L1FastJet_AK5PF.txt")).Data())); 
    jetCorr->AddCorrectionFromFile(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/Summer12_V7_MC_L2Relative_AK5PF.txt")).Data())); 
    jetCorr->AddCorrectionFromFile(std::string((gSystem->Getenv("CMSSW_BASE") + TString("/src/MitPhysics/data/Summer12_V7_MC_L3Absolute_AK5PF.txt")).Data())); 
  }
  jetCorr->SetInputName(pubJet->GetOutputName());
  jetCorr->SetCorrectedName("CorrectedJets");    

  JetIDMod *theJetID = new JetIDMod;
  theJetID->SetInputName(jetCorr->GetOutputName());
  theJetID->SetPtCut(30.0);
  theJetID->SetEtaMaxCut(4.7);
  theJetID->SetJetEEMFractionMinCut(0.00);
  theJetID->SetOutputName("GoodJets");
  theJetID->SetApplyBetaCut(kFALSE);
  theJetID->SetApplyMVACut(kTRUE);

  JetCleaningMod *theJetCleaning = new JetCleaningMod;
  theJetCleaning->SetCleanElectronsName(electronCleaning->GetOutputName());
  theJetCleaning->SetCleanMuonsName(muonIdMod->GetOutputName());
  theJetCleaning->SetCleanPhotonsName(photonCleaningMod->GetOutputName());
  theJetCleaning->SetApplyPhotonRemoval(kTRUE);
  theJetCleaning->SetGoodJetsName(theJetID->GetOutputName());
  theJetCleaning->SetCleanJetsName("CleanJets");
        
  //------------------------------------------------------------------------------------------------
  // select events with photon+MET
  //------------------------------------------------------------------------------------------------
  MonoPhotonAnalysisMod         *phplusmet = new MonoPhotonAnalysisMod("MonoPhotonSelector");
  phplusmet->SetInputPhotonsName(photonCleaningMod->GetOutputName()); //identified photons
  phplusmet->SetInputElectronsName(electronCleaning->GetOutputName()); //identified electrons
  phplusmet->SetInputMuonsName(muonIdMod->GetOutputName()); //muons
  phplusmet->SetPhotonsFromBranch(kFALSE);
  phplusmet->SetElectronsFromBranch(kFALSE);
  phplusmet->SetMuonsFromBranch(kFALSE);
  phplusmet->SetMinNumPhotons(0);
  phplusmet->SetMinNumLeptons(0);
  phplusmet->SetMinPhotonEt(0);
  phplusmet->SetMaxPhotonEta(9.4);
  phplusmet->SetMinMetEt(0);
  
  MonoPhotonAnalysisMod         *dilepton = new MonoPhotonAnalysisMod("MonoPhotonSelector_dilepton");
  dilepton->SetInputPhotonsName(photonCleaningMod->GetOutputName()); //identified photons
  dilepton->SetInputElectronsName(electronCleaning->GetOutputName()); //identified electrons
  dilepton->SetInputMuonsName(muonIdMod->GetOutputName()); //muons
  dilepton->SetPhotonsFromBranch(kFALSE);
  dilepton->SetElectronsFromBranch(kFALSE);
  dilepton->SetMuonsFromBranch(kFALSE);
  dilepton->SetMinNumPhotons(1);
  dilepton->SetMinNumLeptons(2);
  dilepton->SetMinPhotonEt(0);
  dilepton->SetMaxPhotonEta(2.4);
  dilepton->SetMinMetEt(0);

  MonoPhotonAnalysisMod         *phfake = new MonoPhotonAnalysisMod("MonoPhotonSelector_phfake");
  phfake->SetInputPhotonsName(photreg->GetOutputName()); //all photons
  phfake->SetInputElectronsName(electronCleaning->GetOutputName()); //identified electrons
  phfake->SetInputMuonsName(muonIdMod->GetOutputName()); //muons
  phfake->SetPhotonsFromBranch(kFALSE);
  phfake->SetElectronsFromBranch(kFALSE);
  phfake->SetMuonsFromBranch(kFALSE);
  phfake->SetMinNumPhotons(0);
  phfake->SetMinNumLeptons(0);
  phfake->SetMinPhotonEt(0);
  phfake->SetMaxPhotonEta(9.4);
  phfake->SetMinMetEt(0);

  MonoPhotonAnalysisMod         *beamhalo = new MonoPhotonAnalysisMod("MonoPhotonSelector_beamhalo");
  beamhalo->SetInputPhotonsName(photreg->GetOutputName()); //all photons
  beamhalo->SetInputElectronsName(electronCleaning->GetOutputName()); //identified electrons
  beamhalo->SetInputMuonsName(muonIdMod->GetOutputName()); //muons
  beamhalo->SetPhotonsFromBranch(kFALSE);
  beamhalo->SetElectronsFromBranch(kFALSE);
  beamhalo->SetMuonsFromBranch(kFALSE);
  beamhalo->SetMinNumPhotons(1);
  beamhalo->SetMinNumLeptons(0);
  beamhalo->SetMinPhotonEt(100);
  beamhalo->SetMaxPhotonEta(2.4);
  beamhalo->SetMinMetEt(100);

  MonoPhotonTreeWriter *phplusmettree = new MonoPhotonTreeWriter("MonoPhotonTreeWriter");
  phplusmettree->SetPhotonsFromBranch(kFALSE);
  phplusmettree->SetPhotonsName(photonCleaningMod->GetOutputName());
  phplusmettree->SetElectronsFromBranch(kFALSE);
  phplusmettree->SetElectronsName(electronCleaning->GetOutputName());
  phplusmettree->SetMuonsFromBranch(kFALSE);
  phplusmettree->SetMuonsName(muonIdMod->GetOutputName());
  phplusmettree->SetJetsFromBranch(kFALSE);
  phplusmettree->SetJetsName(theJetCleaning->GetOutputName());
  phplusmettree->SetCosmicsFromBranch(kTRUE);
  phplusmettree->SetPVFromBranch(kFALSE);
  phplusmettree->SetPVName(goodPVFilterMod->GetOutputName());
  phplusmettree->SetLeptonsName(merger->GetOutputName());
  phplusmettree->SetHltObjsName(hltModP->GetOutputName());
  phplusmettree->SetIsData(isData);
  phplusmettree->SetProcessID(0);
  phplusmettree->SetFillNtupleType(0);
  
  MonoPhotonTreeWriter *dileptontree = new MonoPhotonTreeWriter("MonoPhotonTreeWriter_dilepton");
  dileptontree->SetPhotonsFromBranch(kFALSE);
  dileptontree->SetPhotonsName(photonCleaningMod->GetOutputName());
  dileptontree->SetElectronsFromBranch(kFALSE);
  dileptontree->SetElectronsName(electronCleaning->GetOutputName());
  dileptontree->SetMuonsFromBranch(kFALSE);
  dileptontree->SetMuonsName(muonIdMod->GetOutputName());
  dileptontree->SetJetsFromBranch(kFALSE);
  dileptontree->SetJetsName(theJetCleaning->GetOutputName());
  dileptontree->SetCosmicsFromBranch(kTRUE);
  dileptontree->SetPVFromBranch(kFALSE);
  dileptontree->SetPVName(goodPVFilterMod->GetOutputName());
  dileptontree->SetLeptonsName(merger->GetOutputName());
  dileptontree->SetHltObjsName(hltModP->GetOutputName());
  dileptontree->SetIsData(isData);
  dileptontree->SetProcessID(0);
  dileptontree->SetFillNtupleType(1);//dilepton

  MonoPhotonTreeWriter *phfaketree = new MonoPhotonTreeWriter("MonoPhotonTreeWriter_phfake");
  phfaketree->SetPhotonsFromBranch(kFALSE);
  phfaketree->SetPhotonsName(photreg->GetOutputName());
  phfaketree->SetElectronsFromBranch(kFALSE);
  phfaketree->SetElectronsName(electronCleaning->GetOutputName());
  phfaketree->SetMuonsFromBranch(kFALSE);
  phfaketree->SetMuonsName(muonIdMod->GetOutputName());
  phfaketree->SetJetsFromBranch(kFALSE);
  phfaketree->SetJetsName(theJetCleaning->GetOutputName());
  phfaketree->SetCosmicsFromBranch(kTRUE);
  phfaketree->SetPVFromBranch(kFALSE);
  phfaketree->SetPVName(goodPVFilterMod->GetOutputName());
  phfaketree->SetLeptonsName(merger->GetOutputName());
  phfaketree->SetHltObjsName(hltModP->GetOutputName());
  phfaketree->SetIsData(isData);
  phfaketree->SetProcessID(0);
  phfaketree->SetFillNtupleType(2);//phfake;

  MonoPhotonTreeWriter *beamhalotree = new MonoPhotonTreeWriter("MonoPhotonTreeWriter_beamhalo");
  beamhalotree->SetPhotonsFromBranch(kFALSE);
  beamhalotree->SetPhotonsName(photreg->GetOutputName());
  beamhalotree->SetElectronsFromBranch(kFALSE);
  beamhalotree->SetElectronsName(electronCleaning->GetOutputName());
  beamhalotree->SetMuonsFromBranch(kFALSE);
  beamhalotree->SetMuonsName(muonIdMod->GetOutputName());
  beamhalotree->SetJetsFromBranch(kFALSE);
  beamhalotree->SetJetsName(theJetCleaning->GetOutputName());
  beamhalotree->SetCosmicsFromBranch(kTRUE);
  beamhalotree->SetPVFromBranch(kFALSE);
  beamhalotree->SetPVName(goodPVFilterMod->GetOutputName());
  beamhalotree->SetLeptonsName(merger->GetOutputName());
  beamhalotree->SetHltObjsName(hltModP->GetOutputName());
  beamhalotree->SetIsData(isData);
  beamhalotree->SetProcessID(0);
  beamhalotree->SetFillNtupleType(3);//beamhalo

  //------------------------------------------------------------------------------------------------
  // making analysis chain
  //------------------------------------------------------------------------------------------------
  // this is how it always starts
  runLumiSel      ->Add(mcselmod);
 
  mcselmod->Add(generatorMod);
  generatorMod->Add(goodPVFilterMod);
  goodPVFilterMod->Add(hltModP);
  // photon regression
  hltModP->Add(photreg);
  // simple object id modules
  photreg          ->Add(SepPUMod); 
  SepPUMod         ->Add(muonIdMod);
  muonIdMod        ->Add(eleIdMod);
  eleIdMod         ->Add(electronCleaning);
  electronCleaning ->Add(merger);
  merger           ->Add(photonIDMod);
  photonIDMod      ->Add(photonCleaningMod);
  photonCleaningMod->Add(pubJet);
  pubJet           ->Add(jetCorr);
  jetCorr          ->Add(theJetID);
  theJetID         ->Add(theJetCleaning);
   
  // Gamma+met selection
  theJetCleaning   ->Add(phplusmet);
  phplusmet        ->Add(phplusmettree);
  
  // Dilepton selection
  theJetCleaning   ->Add(dilepton);
  dilepton         ->Add(dileptontree);

  // Fake photon selection
  theJetCleaning   ->Add(phfake);
  phfake           ->Add(phfaketree);

  // Beam Halo selection
  theJetCleaning   ->Add(beamhalo);
  beamhalo         ->Add(beamhalotree);

  //------------------------------------------------------------------------------------------------
  // setup analysis
  //------------------------------------------------------------------------------------------------
  Analysis *ana = new Analysis;
  ana->SetUseHLT(kTRUE);
  ana->SetKeepHierarchy(kTRUE);
  ana->SetSuperModule(runLumiSel);
  ana->SetPrintScale(100);
  if (nEvents >= 0)
    ana->SetProcessNEvents(nEvents);

  //------------------------------------------------------------------------------------------------
  // organize input
  //------------------------------------------------------------------------------------------------
  Catalog *c = new Catalog(catalogDir);
  TString skimdataset = TString(dataset)+TString("/") +TString(skim);
  Dataset *d = NULL;
  TString bookstr = book;
  if (TString(skim).CompareTo("noskim") == 0)
    d = c->FindDataset(bookstr,dataset,fileset,false);
  else 
    d = c->FindDataset(bookstr,skimdataset.Data(),fileset,false);
  //ana->AddDataset(d);
  //signal
  //ana->AddFile("/mnt/hadoop/cms/store/user/paus/filefi/031/s12-addmpho-md3_d2-v7a/CCD71B15-3C01-E211-A4E0-848F69FD294C.root");
  //fake photons
  ana->AddFile("/mnt/hadoop/cms/store/user/paus/filefi/031/s12-zjets-ptz100-v7a/FE5D99F0-A873-E211-A8BF-002618943923.root");
  ana->AddFile("/mnt/hadoop/cms/store/user/paus/filefi/031/s12-zjets-ptz100-v7a/BAA051FB-A573-E211-8B91-002618943862.root");
  ana->AddFile("/mnt/hadoop/cms/store/user/paus/filefi/031/s12-zjets-ptz100-v7a/3EE09ED4-A273-E211-A62B-003048FFD760.root");
  ana->AddFile("/mnt/hadoop/cms/store/user/paus/filefi/031/s12-zjets-ptz100-v7a/0813BE62-AD73-E211-AE30-003048679166.root");

  //------------------------------------------------------------------------------------------------
  // organize output
  //------------------------------------------------------------------------------------------------
  TString rootFile = TString(outputName);
  rootFile += TString("_") + TString(dataset) + TString("_") + TString(skim);
  if (TString(fileset) != TString(""))
    rootFile += TString("_") + TString(fileset);
  rootFile += TString(".root");
  ana->SetOutputName(rootFile.Data());
  //ana->SetCacheSize(64*1024*1024);
  ana->SetCacheSize(0);

  //------------------------------------------------------------------------------------------------
  // Say what we are doing
  //------------------------------------------------------------------------------------------------
  printf("\n==== PARAMETER SUMMARY FOR THIS JOB ====\n");
  printf("\n JSON file: %s\n  and overlap cut: %f (%s)\n",jsonFile.Data(),overlapCut,overlap);
  printf("\n Rely on Catalog: %s\n",catalogDir);
  printf("  -> Book: %s  Dataset: %s  Skim: %s  Fileset: %s <-\n",book,dataset,skim,fileset);
  printf("\n Root output: %s\n\n",rootFile.Data());  
  printf("\n========================================\n");

  //------------------------------------------------------------------------------------------------
  // run the analysis after successful initialisation
  //------------------------------------------------------------------------------------------------
  ana->Run(!gROOT->IsBatch());

  return;
}
