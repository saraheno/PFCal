#include<string>
#include<iostream>
#include<fstream>
#include<sstream>
#include<map>
#include <boost/algorithm/string.hpp>
#include "boost/lexical_cast.hpp"
#include "boost/program_options.hpp"
#include "boost/format.hpp"
#include "boost/function.hpp"

#include "TFile.h"
#include "TTree.h"
#include "TChain.h"
#include "TH3F.h"
#include "TH2F.h"
#include "TH1F.h"
#include "TF1.h"
#include "TStyle.h"
#include "TCanvas.h"
#include "TLatex.h"
#include "TRandom3.h"
#include "TMatrixD.h"
#include "TMatrixDSym.h"
#include "TVectorD.h"

#include "HGCSSEvent.hh"
#include "HGCSSInfo.hh"
#include "HGCSSSamplingSection.hh"
#include "HGCSSSimHit.hh"
#include "HGCSSRecoHit.hh"
#include "HGCSSGenParticle.hh"
#include "HGCSSParameters.hh"
#include "HGCSSCalibration.hh"
#include "HGCSSDigitisation.hh"
#include "HGCSSDetector.hh"
#include "HGCSSGeometryConversion.hh"
#include "HGCSSPUenergy.hh"
#include "HGCSSSimpleHit.hh"

#include "PositionFit.hh"
#include "SignalRegion.hh"

#include "Math/Vector3D.h"
#include "Math/Vector3Dfwd.h"
#include "Math/Point2D.h"
#include "Math/Point2Dfwd.h"

#include "utilities.h"

using boost::lexical_cast;
namespace po=boost::program_options;

void SNAPrec(TH2F* h_1,std::vector<HGCSSRecoHit> *rechitvec) {
  std::cout<<"SNAP"<<std::endl;
    for (unsigned iH(0); iH<(*rechitvec).size(); ++iH){//loop on hits
      HGCSSRecoHit lHit = (*rechitvec)[iH];
      double xh=lHit.get_x();
      double yh=lHit.get_y();
      double zh=lHit.get_z();
      double rh=sqrt(xh*xh+yh*yh);
      double Eh=lHit.energy();
      h_1->Fill(zh,rh,Eh);
    }

  return;
}

void SNAPsim(TH2F* h_1,std::vector<HGCSSSimHit> *simhitvec,
	     HGCSSDetector & myDetector,
	     HGCSSGeometryConversion & aGeom,
	     unsigned shape
	     ) {
  std::cout<<"SNAP"<<std::endl;
    for (unsigned iH(0); iH<(*simhitvec).size(); ++iH){//loop on hits
      HGCSSSimHit lHit = (*simhitvec)[iH];
      unsigned layer = lHit.layer();
      const HGCSSSubDetector & subdet = myDetector.subDetectorByLayer(layer);
      ROOT::Math::XYZPoint pp = lHit.position(subdet,aGeom,shape);
      double Eh=lHit.energy();
      //std::cout<<Eh<<" "<<pp.z()<<" "<<pp.r()<<std::endl;
      h_1->Fill(pp.z(),pp.r(),Eh);
    }

  return;
}



double DeltaR(double eta1,double phi1,double eta2,double phi2){
  double dr=99999.;
  double deta=fabs(eta1-eta2);
  double dphi=fabs(phi1-phi2);
  if(dphi>TMath::Pi()) dphi=2.*TMath::Pi()-dphi;
  dr=sqrt(deta*deta+dphi*dphi);
  return dr;
}


int main(int argc, char** argv){//main  


  //Input output and config options
  std::string cfg;
  unsigned pNevts;
  std::string outFilePath;
  std::string filePath;
  std::string digifilePath;
  unsigned nRuns;
  std::string simFileName;
  std::string recoFileName;
  unsigned debug;
  po::options_description preconfig("Configuration"); 
  preconfig.add_options()("cfg,c",po::value<std::string>(&cfg)->required());
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(preconfig).allow_unregistered().run(), vm);
  po::notify(vm);
  po::options_description config("Configuration");
  config.add_options()
    //Input output and config options //->required()
    ("pNevts,n",       po::value<unsigned>(&pNevts)->default_value(0))
    ("outFilePath,o",  po::value<std::string>(&outFilePath)->required())
    ("filePath,i",     po::value<std::string>(&filePath)->required())
    ("digifilePath", po::value<std::string>(&digifilePath)->default_value(""))
    ("simFileName,s",  po::value<std::string>(&simFileName)->required())
    ("recoFileName,r", po::value<std::string>(&recoFileName)->required())
    ("nRuns",        po::value<unsigned>(&nRuns)->default_value(0))
    ("debug,d",        po::value<unsigned>(&debug)->default_value(0))
    ;
  po::store(po::command_line_parser(argc, argv).options(config).allow_unregistered().run(), vm);
  po::store(po::parse_config_file<char>(cfg.c_str(), config), vm);
  po::notify(vm);


  std::string inFilePath = filePath+simFileName;

  std::cout << " -- Input parameters: " << std::endl
            << " -- Input file path: " << filePath << std::endl
            << " -- Digi Input file path: " << digifilePath << std::endl
    	    << " -- Output file path: " << outFilePath << std::endl
	    << std::endl
	    << " -- Processing ";
  if (pNevts == 0) std::cout << "all events." << std::endl;
  else std::cout << pNevts << " events per run." << std::endl;

  TRandom3 lRndm(0);
  std::cout << " -- Random number seed: " << lRndm.GetSeed() << std::endl;


  //
  // hardcoded
  //


  //global threshold to reduce size of noise hits
  const double threshMin = 0.5;

  std::cout << " ---- Selection settings: ---- " << std::endl
	    << " -------threshMin " << threshMin << std::endl
	    << " ------------------------------------------" << std::endl;



  /////////////////////////////////////////////////////////////
  //input
  /////////////////////////////////////////////////////////////


  std::ostringstream inputsim;
  inputsim << filePath << "/" << simFileName;
  std::ostringstream inputrec;
  if (digifilePath.size()==0)
    inputrec << filePath << "/" << recoFileName;
  else
    inputrec << digifilePath << "/" << recoFileName;

  std::cout << inputsim.str() << " " << inputrec.str() << std::endl;

  HGCSSInfo * info;

  TChain *lSimTree = new TChain("HGCSSTree");
  TChain *lRecTree = 0;

  TFile * simFile = 0;
  TFile * recFile = 0;

  if (recoFileName.find("Digi") != recoFileName.npos)
    lRecTree = new TChain("RecoTree");
  else lRecTree = new TChain("PUTree");

  if (nRuns == 0){
    if (!testInputFile(inputsim.str(),simFile)) return 1;
    lSimTree->AddFile(inputsim.str().c_str());
    if (simFile) info =(HGCSSInfo*)simFile->Get("Info");
    else {
      std::cout << " -- Error in getting information from simfile!" << std::endl;
      return 1;
    }
    if (!testInputFile(inputrec.str(),recFile)) return 1;
    lRecTree->AddFile(inputrec.str().c_str());
  }
  else {
    for (unsigned i(0);i<nRuns;++i){
      std::ostringstream lstrsim;
      std::ostringstream lstrrec;
      lstrsim << inputsim.str() << "_run" << i << ".root";
      if (testInputFile(lstrsim.str(),simFile)){
        if (simFile) info =(HGCSSInfo*)simFile->Get("Info");
        else {
	  std::cout << " -- Error in getting information from simfile!" << std::endl;
          return 1;
        }
      }
      else continue;
      lstrrec << inputrec.str() << "_run" << i << ".root";
      if (!testInputFile(lstrrec.str(),recFile)) continue;
      lSimTree->AddFile(lstrsim.str().c_str());
      lRecTree->AddFile(lstrrec.str().c_str());
    }
  }

  if (!lSimTree){
    std::cout << " -- Error, tree HGCSSTree cannot be opened. Exiting..." << std::endl;
    return 1;
  }

  if (!lRecTree){
    std::cout << " -- Error, tree RecoTree cannot be opened. Exiting..." << std::endl;
    return 1;
  }





  //assert(info);

  const unsigned versionNumber = info->version();
  const unsigned model = info->model();
  const unsigned shape = info->shape();
  const double cellSize = info->cellSize();
  const double calorSizeXY = info->calorSizeXY();

  bool isTBsetup = (model != 2);
  bool bypassR = false;
  if (isTBsetup) bypassR = true;

  HGCSSDetector & myDetector = theDetector();
  myDetector.buildDetector(versionNumber,true,false,bypassR);

  //corrected for Si-Scint overlap
  const unsigned nLayers = 52;//


  std::cout << " -- Calor size XY = " << calorSizeXY
	    << ", version number = " << versionNumber 
	    << ", model = " << model << std::endl
	    << " -- cellSize = " << cellSize
	    << ", shape = " << shape
	    << ", nLayers = " << nLayers
	    << std::endl;
  HGCSSGeometryConversion geomConv(model,cellSize,bypassR,3);

  geomConv.setXYwidth(calorSizeXY);
  geomConv.setVersion(versionNumber);
  
  if (shape==2) geomConv.initialiseDiamondMap(calorSizeXY,10.);
  else if (shape==3) geomConv.initialiseTriangleMap(calorSizeXY,10.*sqrt(2.));
  else if (shape==1) geomConv.initialiseHoneyComb(calorSizeXY,cellSize);
  else if (shape==4) geomConv.initialiseSquareMap(calorSizeXY,10.);

  //square map for BHCAL
  geomConv.initialiseSquareMap1(1.4,3.0,0,2*TMath::Pi(),0.01745);//eta phi segmentation
  geomConv.initialiseSquareMap2(1.4,3.0,0,2*TMath::Pi(),0.02182);//eta phi segmentation
  std::vector<unsigned> granularity;
  granularity.resize(myDetector.nLayers(),1);
  geomConv.setGranularity(granularity);
  geomConv.initialiseHistos();

  //////////////////////////////////////////////////
  //////////////////////////////////////////////////
  ///////// Output File // /////////////////////////
  //////////////////////////////////////////////////
  //////////////////////////////////////////////////

  TFile *outputFile = TFile::Open(outFilePath.c_str(),"RECREATE");
  
  if (!outputFile) {
    std::cout << " -- Error, output file " << outFilePath << " cannot be opened. Please create output directory. Exiting..." << std::endl;
    return 1;
  }
  else {
    std::cout << " -- output file " << outputFile->GetName() << " successfully opened." << std::endl;
  }
  outputFile->cd();

  const int nsnap=10;
  TH2F *h_snapr[nsnap];
  TH2F *h_snaps[nsnap];
  for(unsigned ii(0);ii<nsnap;ii++) {
    std::ostringstream label;
    label.str("");
    label<<"E-weighted r-z of reco snap "<<ii;
    h_snapr[ii]=new TH2F(label.str().c_str(),"E-weighted r-z of reco hit",500,3100.,5200.,2500,0.,2500.);
    std::ostringstream label2;
    label2.str("");
    label2<<"E-weighted r-z of sim snap "<<ii;
    h_snaps[ii]=new TH2F(label2.str().c_str(),"E-weighted r-z of sim hit",500,300.,500.,2500,250.,700.);

  }

  TH1F* h_energy = new TH1F("h_energy","hit energy",1000,0.,5.);
  TH1F* h_z = new TH1F("h_z","z of hit",5000,3100.,5200);
  TH1F* h_z1 = new TH1F("h_z1","z of hit",5000,3150.,3550);
  TH1F* h_z2 = new TH1F("h_z2","z of hit",5000,3550.,5200);
  TH2F* h_xy = new TH2F("h_xy","xy of hit",1000,-2000,2000,1000,-2000,2000);
  TH1F* h_phibad = new TH1F("h_phibad","gen phi of large neg hits",2000,-3.2,0);
  TH1F* h_xbad = new TH1F("h_xbad","gen x of large neg hits",2000,-2500,2500);
  TH1F* h_ybad = new TH1F("h_ybad","gen y of large neg hits",2000,-2500,2500);
  TH2F* h_xrgood = new TH2F("h_xrgood"," x vs r good",1000,0.,2500.,1000,-2500.,2500.);
  TH2F* h_yrgood = new TH2F("h_yrgood"," y vs r good",1000,0.,2500.,1000,-2500.,2500.);
  TH2F* h_etaphi = new TH2F("h_etaphi","etaphi of hit",1000,1,3.5,1000,-7,7);
  TH2F* h_getaphi = new TH2F("h_getaphi","gen part etaphi of hit",1000,1,3.5,1000,-7,7);
  TH1F* h_l = new TH1F("h_l","layer of hit",80,0.,80.);
  TH1F* h_l2 = new TH1F("h_l2","layer of hit",30,50,80.);
  TH2F* h_zl = new TH2F("h_zl","z vs l of hit",5000,4300.,5200,25,30.,55.);


  TH2F *h_sxy[16];
  TH2F *h_nsxy[16];
  for(unsigned ii(0);ii<16;ii++) {
    std::ostringstream label;
    label.str("");
    label<<"xy of hit scint "<<ii+36;
    h_sxy[ii]=new TH2F(label.str().c_str(),"xy of hit scint",2500,-2500.,2500.,2500,-2500.,2500.);
    std::ostringstream label2;
    label2.str("");
    label2<<"xy of hit not scint "<<ii+36;
    h_nsxy[ii]=new TH2F(label2.str().c_str(),"xy of hit not scint",
        2500,-2500.,2500.,2500,-2500.,2500.);
  }

  TH2F* h_Egenreco = new TH2F("h_Egenreco","E reco sum versus gen",1000,0.,1000.,100,0.,20.);
  TH1F* h_egenreco = new TH1F("h_egenreco","E reco sum over gen",100,0.,2.);
  TH1F* h_egenrecopphi = new TH1F("h_egenrecopphi","E reco sum over gen plus phi",100,0.,2.);
  TH1F* h_egenrecomphi = new TH1F("h_egenrecomphi","E reco sum over gen minus phi",100,0.,2.);


  TH2F* h_EpCone = new TH2F("h_EpCone","Ereco/gen versus cone size",10,0.,1.,100,0.,2.);
  TH2F* h_EpPhi = new TH2F("h_EpPhi","Ereco/gen versus phi",100,-4.,4.,100,0.,2.);
  TH2F* h_etagenmax= new TH2F("h_etagenmax","eta gen vs max",100,1.,5.,100,1.,5.);
  TH2F* h_phigenmax= new TH2F("h_phigenmax","phi gen vs max",100,-4,4.,100,-4.,4.);
  TH1F* h_phimax = new TH1F("h_phimax","phi of max hit",100,-4.,4.);
  TH1F* h_etamax = new TH1F("h_etamax","eta of max hit",100,1.,5.);
  TH1F* h_maxE = new TH1F("h_maxE","energy of highest energy hit",1000,0.,1000.);
  TH1F* h_ECone03 = new TH1F("h_ECone03","Sum energy cone 03",1000,0.,500.);

  TH2F* h_ssvec = new TH2F("h_ssvec","ssvec versus layer number", 70,0.,70.,100,0.,100.);
  TH1F* h_cellid = new TH1F("h_cellid","cell is",25000,0.,250000.);
  TH1F* h_cellids = new TH1F("h_cellids","scint cell is",25000,0.,5000000000.);
  TH2F* h_cellidz = new TH2F("h_cellidz","cell is versus z",5000,3100,5200,1000,0.,250000.);

  TH2F* h_banana = new TH2F("h_banana","banana plot",1000,0.,500.,1000,0.,500.);
  TH1F* h_fracBH = new TH1F("h_fracBH","fraction in BH",100,-01.,1.1);
  
  ///////////////////////////////////////////////////////
  //////////////////  start event loop
  //////////////////////////////////////////////////////

  //  const unsigned nEvts = ((pNevts > lRecTree->GetEntries() || pNevts==0) ? static_cast<unsigned>(lRecTree->GetEntries()) : pNevts) ;
  
  //std::cout << " -- Processing " << nEvts << " events out of " << 
  //   lRecTree->GetEntries()<< std::endl;


  const unsigned nEvts = ((pNevts > lSimTree->GetEntries() || pNevts==0) ? static_cast<unsigned>(lSimTree->GetEntries()) : pNevts) ;

  std::cout << " -- Processing " << nEvts << " events out of " << lSimTree->GetEntries() << " " << lRecTree->GetEntries() << std::endl;


  //loop on events
  HGCSSEvent * event = 0;
  HGCSSEvent * eventRec = 0;
  std::vector<HGCSSSamplingSection> * ssvec = 0;
  std::vector<HGCSSSimHit> * simhitvec = 0;
  std::vector<HGCSSRecoHit> * rechitvec = 0;
  std::vector<HGCSSGenParticle> * genvec = 0;
  unsigned nPuVtx = 0;



  lSimTree->SetBranchAddress("HGCSSEvent",&event);
  lSimTree->SetBranchAddress("HGCSSSamplingSectionVec",&ssvec);
  lSimTree->SetBranchAddress("HGCSSSimHitVec",&simhitvec);
  lSimTree->SetBranchAddress("HGCSSGenParticleVec",&genvec);

  lRecTree->SetBranchAddress("HGCSSEvent",&eventRec);
  lRecTree->SetBranchAddress("HGCSSRecoHitVec",&rechitvec);
  if (lRecTree->GetBranch("nPuVtx")) lRecTree->SetBranchAddress("nPuVtx",&nPuVtx);



  unsigned ievtRec = 0;
  unsigned nSkipped = 0;
  std::vector<double> absW;
  bool firstEvent = true;
  unsigned ibinScintmin=1000000;
  unsigned ibinScintmax=0;
  
  double rmaxs[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double rmins[16]={5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000};
  double rmaxns[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double rminns[16]={5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000};


  double xmax[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double xmin[16]={5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000};
  double ymax[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double ymin[16]={5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000,5000};
  
  int isnap=-1;
  for (unsigned ievt(0); ievt<nEvts; ++ievt){//loop on entries
    if (ievtRec>=lRecTree->GetEntries()) continue;


    if (debug) std::cout << std::endl<<std::endl<<"... Processing entry: " << ievt << std::endl;
    else if (ievt%50 == 0) std::cout << "... Processing entry: " << ievt << std::endl;



    lSimTree->GetEntry(ievt);
    lRecTree->GetEntry(ievtRec);
    if (nPuVtx>0 && eventRec->eventNumber()==0 && event->eventNumber()!=0) {
      std::cout << " skip !" << ievt << " " << ievtRec << std::endl;
      nSkipped++;
      continue;
    }


    if(firstEvent) {
      firstEvent=false;
      std::cout<<" size of ssvec of weights is "<<(*ssvec).size()<<std::endl;
      double absweight=0;      
      for (unsigned iL(0); iL<(*ssvec).size();++iL){
	if(iL<((*ssvec).size()-1)) {
	  unsigned next=iL+1;
	  absweight=(((*ssvec)[iL].voldEdx())+((*ssvec)[next].voldEdx()))/2. ;
	} else{
	  absweight+=(*ssvec)[iL].voldEdx()  ;
	}
	absW.push_back(absweight);
	absweight=0;
      }
      std::cout << " -- AbsWeight size: " << absW.size() << std::endl;
      std::cout<<" values are ";
      for (unsigned iL(0); iL<(*ssvec).size();++iL){
	std::cout<<" "<<absW[iL];
      }
      std::cout<<std::endl;
    }
    for (unsigned iL(0); iL<(*ssvec).size();++iL){
      h_ssvec->Fill(iL,absW[iL]);
    }

    //
    // information about generator level particles

    double ptgen=-1.;
    double Egen=-1.;
    double ptgenpx=-1.;
    double ptgenpy=-1.;
    double ptgenpz=-1.;
    double etagen=99999.;
    double phigen=99999.;
    double thetagen=-1.;
    int pidgen=-1;
    if((*genvec).size()>0) {
      pidgen=(*genvec)[0].pdgid();
      ptgenpx=(*genvec)[0].px()/1000.;
      ptgenpy=(*genvec)[0].py()/1000.;
      ptgenpz=(*genvec)[0].pz()/1000.;
      ptgen=sqrt(ptgenpx*ptgenpx+ptgenpy*ptgenpy);
      etagen=(*genvec)[0].eta();
      Egen=sqrt(ptgenpx*ptgenpx+ptgenpy*ptgenpy+ptgenpz*ptgenpz);
      phigen=(*genvec)[0].phi();
      thetagen=(*genvec)[0].theta();
    }
    h_getaphi->Fill(etagen,phigen);
    if(debug) {
      std::cout<<" gen vec size is "<<(*genvec).size()<<std::endl;
      std::cout<<" first gen "<<ptgen<<" "<<Egen<<" "<<pidgen<<" "<<etagen<<" "<<phigen<<std::endl;
      for (unsigned iP(0); iP<(*genvec).size(); ++iP){
        std::cout<<" gen particle "<<iP<<" is "<<(*genvec)[iP].pdgid()<<std::endl;
      }
    }


    bool isScint = false;
    if (debug) std::cout << " - Event contains " << (*rechitvec).size() << " rechits." << std::endl;

    // make some simple plots about all the raw rechits, without the weights
    unsigned iMax=-1;
    double MaxE=-1.;
    bool snap=false;
    for (unsigned iH(0); iH<(*rechitvec).size(); ++iH){//loop on hits
      HGCSSRecoHit lHit = (*rechitvec)[iH];
      double xh=lHit.get_x();
      double yh=lHit.get_y();
      double zh=lHit.get_z();
      double rh=sqrt(xh*xh+yh*yh);
      double Eh=lHit.energy();
      double leta = lHit.eta();
      double lphi = lHit.phi();
      unsigned layer = lHit.layer();
      unsigned ixx=layer;
      if(ixx>52) ixx=ixx-17;
      int ip=ixx-36;
      const HGCSSSubDetector & subdet = myDetector.subDetectorByLayer(layer);
      isScint = subdet.isScint;
      TH2Poly *map = 
	isScint?(subdet.type==DetectorEnum::BHCAL1?geomConv.squareMap1()
                                                : geomConv.squareMap2())
        : shape==4?geomConv.squareMap() 
	: shape==2?geomConv.diamondMap() 
	: shape==3? geomConv.triangleMap()
	: geomConv.hexagonMap();
      unsigned cellid = map->FindBin(xh,yh);
      geomConv.fill(layer,Eh,0,cellid,zh);

      if(Eh>MaxE) {MaxE=Eh; iMax=iH;}
      if (debug>20) std::cout << " -- hit " << iH << " eta " << leta << std::endl; 

      h_cellid->Fill(cellid);
      if(isScint) {
	if(cellid>ibinScintmax) ibinScintmax=cellid;
	if(cellid<ibinScintmin) ibinScintmin=cellid;
	h_cellids->Fill(cellid);
      }
      h_cellidz->Fill(zh,cellid);


      h_energy->Fill(Eh);
      h_z->Fill(zh);
      h_z1->Fill(zh);
      h_z2->Fill(zh);
      h_l->Fill(layer+0.5);


      h_zl->Fill(zh,ixx);
      h_l2->Fill(layer+0.5);
      h_xy->Fill(xh,yh);
      h_etaphi->Fill(leta,lphi);



      if(ip>=0) {
	if(debug>4) {
	  if(xh>5000) std::cout<<"large xh "<<xh<<" phigen is "<<phigen<<" yh rh zh are"<<yh<<" "<<rh<<" "<<zh<<std::endl;
	}
	if(xh>5000) {
	  if(!snap) {
	  if(isnap<nsnap-1) {
	    snap=true;
	    isnap+=1;
	    std::cout<<" isnap is "<<isnap<<std::endl;
	    SNAPrec(h_snapr[isnap],rechitvec);
	    SNAPsim(h_snaps[isnap],simhitvec,myDetector,geomConv,shape);
	  }
	  }
	  double rr=zh*tan(thetagen);
	  double xx=rr*cos(phigen);
	  double yy=rr*sin(phigen);
	  h_phibad->Fill(phigen);
	  h_xbad->Fill(xx);
	  h_ybad->Fill(yy);
	} else {
	  
	  h_xrgood->Fill(rh,xh);
	  h_yrgood->Fill(rh,yh);
	}
       if(xh<xmin[ip]) {xmin[ip]=xh;}
       if(xh>xmax[ip]) {xmax[ip]=xh;}
       if(yh<ymin[ip]) {ymin[ip]=yh;}
       if(yh>ymax[ip]) {ymax[ip]=yh;}

	if(isScint) {
	  //	  std::cout<<"ip rh is "<<ip<<" "<<rh<<std::endl;
	  //std::cout<<rmins[ip]<<" "<<rmaxs[ip]<<std::endl;
	  h_sxy[ip]->Fill(xh,yh);
	  if(rh<rmins[ip]) {rmins[ip]=rh;}
	  if(rh>rmaxs[ip]) {rmaxs[ip]=rh;}
	  //std::cout<<rmins[ip]<<" "<<rmaxs[ip]<<std::endl;
	} else {
	  h_nsxy[ip]->Fill(xh,yh);
	  if(rh<rminns[ip]) {rminns[ip]=rh;}
	  if(rh>rmaxns[ip]) {rmaxns[ip]=rh;}
	}
      }


    }//loop on hits



    HGCSSRecoHit lHit = (*rechitvec)[iMax];
    double maxeta = lHit.eta();
    double maxphi=lHit.phi();
    double maxE=lHit.energy();
    h_maxE->Fill(maxE);
    h_etagenmax->Fill(maxeta,etagen);
    h_phigenmax->Fill(maxphi,phigen);
    h_phimax->Fill(maxphi);
    h_etamax->Fill(maxeta);
    if(debug>2) {
      std::cout<<" Max hit energy eta phi "<<lHit.energy()<<" "<<lHit.eta()<<" "<<lHit.phi()<<std::endl;
    }

    // make e/p plots for various cones around gen particle, using weights this time
    // for now, scale up by 10.  don't know why.  asking Anne-Marie
    // also change to GeV for this section
    const unsigned isize=5;
    double coneSize[isize]={0.1,0.2,0.3,0.4,0.5};
    double rechitsum[isize]={0.,0.,0.,0.,0.};
    double rechitBHsum[isize]={0.,0.,0.,0.};

    double etaW=0.;
    double phiW=0.;
    double norm=0.;
    double etaaxis=etagen;
    double phiaxis=phigen;
    //double etaaxis=maxeta;
    //double phiaxis=maxphi;
    for (unsigned iH(0); iH<(*rechitvec).size(); ++iH){//loop on hits
      HGCSSRecoHit lHit = (*rechitvec)[iH];
      unsigned layer = lHit.layer();
      double leta = lHit.eta();
      double lphi = lHit.phi();
      //if(lphi<0) lphi=2.*TMath::Pi()+lphi;
      double lenergy=lHit.energy()*absW[layer]/1000.;
      if (debug>20) std::cout << " -- hit " << iH << " et eta phi " << lenergy<<" "<<leta << " "<< lphi<<std::endl; 
	//clean up rechit collection

      const HGCSSSubDetector & subdet = myDetector.subDetectorByLayer(layer);
      isScint = subdet.isScint;

      norm+=lenergy;
      etaW+=leta*lenergy;
      phiW+=lphi*lenergy;

      double dR=DeltaR(etaaxis,phiaxis,leta,lphi);
      //double dR=fabs(etagen-leta);
      if(debug>20) std::cout<<" dR "<<dR<<" "<<etagen<<" "<<phigen<<" "<<leta<<" "<<lphi<<std::endl;
      for(unsigned ii(0);ii<isize;ii++) {
	if(dR<coneSize[ii]) {
	  rechitsum[ii]+=lenergy;
	  if(isScint) rechitBHsum[ii]+=lenergy;
	}
      }


    }//loop on hits
    if(debug>1) {
      std::cout<<" reco gen are "<<rechitsum[0]<<" "<<rechitsum[1]<<" "<<rechitsum[2]<<" "<<rechitsum[3]<<" "<<rechitsum[4]<<" "<<Egen<<std::endl;
    }
    if(debug>5) std::cout<<"weighted eta phi are "<<etaW/norm<<" "<<phiW/norm<<std::endl;

    h_Egenreco->Fill(Egen,rechitsum[3]/Egen);
    h_egenreco->Fill(rechitsum[3]/Egen);
    if(phigen>0) h_egenrecopphi->Fill(rechitsum[3]/Egen);
    else h_egenrecomphi->Fill(rechitsum[3]/Egen);
    h_EpPhi->Fill(phigen,rechitsum[3]/Egen);
    for(unsigned ii(0);ii<isize;ii++) {
      h_EpCone->Fill(coneSize[ii],rechitsum[ii]/Egen);
    }
    h_banana->Fill(rechitsum[3]-rechitBHsum[3],rechitBHsum[3]);
    double frac=-0.05;
    double notBH=rechitsum[3]-rechitBHsum[3];
    if(rechitsum[3]>0) frac=rechitBHsum[3]/rechitsum[3];
    h_fracBH->Fill(frac);
    

    h_ECone03->Fill(rechitsum[3]);
      //miptree->Fill();

    geomConv.initialiseHistos();
    ievtRec++;
  }//loop on entries

    if(debug) {
      std::cout<<std::endl<<std::endl<<" min max dif scint cell is are "<<ibinScintmin<<" "<<ibinScintmax<<" "<<ibinScintmax-ibinScintmin<<std::endl;
      std::cout<<std::endl<<std::endl<<"rmin max for layer"<<std::endl;;
      for(unsigned ii(0);ii<16;ii++){
	std::cout<<"   "<<ii+36<<" "<<rminns[ii]<<" "<<rmaxns[ii]<<" "<<rmins[ii]<<" "<<rmaxs[ii]<<std::endl;
      }
      std::cout<<std::endl<<std::endl<<"xmin max for layer"<<std::endl;;
      for(unsigned ii(0);ii<16;ii++){
	std::cout<<"   "<<ii+36<<" "<<xmin[ii]<<" "<<xmax[ii]<<std::endl;
      }
      std::cout<<std::endl<<std::endl<<"ymin max for layer"<<std::endl;;
      for(unsigned ii(0);ii<16;ii++){
	std::cout<<"   "<<ii+36<<" "<<ymin[ii]<<" "<<ymax[ii]<<std::endl;
      }
    }



  if(debug) std::cout<<"writing files"<<std::endl;

  outputFile->cd();
  outputFile->Write();
  outputFile->Close();

  return 0;
  
}//main
