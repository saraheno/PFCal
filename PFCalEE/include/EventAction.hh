#ifndef EventAction_h
#define EventAction_h 1

#include "SamplingSection.hh"

#include "G4ThreeVector.hh"
#include "G4UserEventAction.hh"
#include "globals.hh"

#include "TFile.h"
#include "TNtuple.h"

#include <map>

class RunAction;
class EventActionMessenger;

class EventAction : public G4UserEventAction
{
public:
  EventAction();
  virtual ~EventAction();
  void BeginOfEventAction(const G4Event*);
  void EndOfEventAction(const G4Event*);
  void Detect(G4double edep, G4double stepl,G4double globalTime, G4int pdgId, G4VPhysicalVolume *volume);
  void SetPrintModulo(G4int    val)  {printModulo = val;};
  void Add( std::vector<SamplingSection> *newDetector ) { detector_=newDetector; }

private:
  RunAction*  runAct;
  std::vector<SamplingSection> *detector_;
  G4int     evtNb_,printModulo;
  TFile *outF_;
  TNtuple *ntuple_;
  Float_t event_[15];
  EventActionMessenger*  eventMessenger;
};

#endif

    