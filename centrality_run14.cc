
#include "centrality_run14.h"

#include <iostream>
#include <math.h>

CentralityRun14::CentralityRun14()
    : refmultcorr_(-1.0), centrality_16_(-1), centrality_9_(-1), weight_(1.0),
      min_vz_(0.0), max_vz_(0.0), min_zdc_(0.0), max_zdc_(0.0), min_run_(0),
      max_run_(0), weight_bound_(0), vz_norm_(0), zdc_norm_(0),
      smoothing_(true) {

  dis_ = std::uniform_real_distribution<double>(0.0, 1.0);
}

CentralityRun14::~CentralityRun14() {}

void CentralityRun14::loadCentralityDef(CentDefId id) {
  CentralityDef &def = CentralityDef::instance();

  setRunRange(def.runIdMin(id), def.runIdMax(id));
  setVzRange(def.vzMin(id), def.vzMax(id));
  setZDCRange(def.zdcMin(id), def.zdcMax(id));
  setVzNormalizationPoint(def.vzNormPoint(id));
  setZDCNormalizationPoint(def.zdcNormPoint(id));
  setZDCParameters(def.zdcParameters(id));
  setVzParameters(def.vzParameters(id));
  setWeightParameters(def.weightParameters(id), def.weightBound(id));
  setCentralityBounds16Bin(def.centralityBounds(id));
}

void CentralityRun14::setEvent(int runid, double refmult, double zdc, double vz) {
  if (checkEvent(runid, refmult, zdc, vz)) {
    calculateCentrality(refmult, zdc, vz);
  }
  // if event isn't in the run ID range, isn't in the vz range, or luminosity
  // range, set refmultcorr = refmult
  else {
    refmultcorr_ = refmult;
    centrality_9_ = -1;
    centrality_16_ = -1;
    weight_ = 1.0;
  }
}

void CentralityRun14::setZDCParameters(double par0, double par1) {
  zdc_par_ = std::vector<double>{par0, par1};
}
void CentralityRun14::setZDCParameters(const std::vector<double> &pars) {
  zdc_par_.clear();

  if (pars.size() != 2) {
    std::cerr << "zdc correction currently implemented as a linear fit "
              << "but " << pars.size() << " parameters were passed, not 2 "
              << std::endl;
    return;
  }
  zdc_par_ = pars;
}
void CentralityRun14::setVzParameters(double par0, double par1, double par2,
                                 double par3, double par4, double par5,
                                 double par6) {
  vz_par_ = std::vector<double>{par0, par1, par2, par3, par4, par5, par6};
}

void CentralityRun14::setVzParameters(const std::vector<double> &pars) {
  vz_par_.clear();

  if (pars.size() != 7) {
    std::cerr
        << "vz correction currently implemented as a 6th order polynomial "
        << "but " << pars.size() << " parameters were passed, not 7 "
        << std::endl;
    ;
    return;
  }
  vz_par_ = pars;
}

void CentralityRun14::setCentralityBounds16Bin(const std::vector<unsigned> &bounds) {
  cent_bin_16_.clear();
  cent_bin_9_.clear();

  if (bounds.size() != 16) {
    std::cerr << "centrality bounds require 16 parameters, but only "
              << bounds.size() << " parameters were passed" << std::endl;
    return;
  }

  for (unsigned i = 0; i < bounds.size(); ++i) {
    cent_bin_16_.push_back(bounds[i]);
    if (i % 2 == 0 || i == 15) {
      cent_bin_9_.push_back(bounds[i]);
    }
  }
}

void CentralityRun14::setWeightParameters(const std::vector<double> &pars,
                                     double bound) {
  weight_par_.clear();
  weight_bound_ = 0;
  if (pars.size() != 7) {
    std::cerr << "reweighting require 7 parameters, but only " << pars.size()
              << " parameters were passed" << std::endl;
    return;
  }
  weight_par_ = pars;
  weight_bound_ = bound;
}

bool CentralityRun14::isValid() {
  if (max_run_ <= 0 || weight_par_.size() != 7 || vz_par_.size() != 7 ||
      zdc_par_.size() != 2 || cent_bin_16_.size() != 16 ||
      (max_zdc_ <= min_zdc_) || (max_vz_ <= min_vz_))
    return false;
  return true;
}

bool CentralityRun14::checkEvent(int runid, double refmult, double zdc, double vz) {
  if (refmult < 0)
    return false;
  if (max_run_ > 0 && (runid < min_run_ || runid > max_run_))
    return false;
  if (vz < min_vz_ || vz > max_vz_)
    return false;
  if (zdc < min_zdc_ || zdc > max_zdc_)
    return false;
  return true;
}

void CentralityRun14::calculateCentrality(double refmult, double zdc, double vz) {

  // we randomize raw refmult within 1 bin to avoid the peaky structures at low
  // refmult
  double raw_ref = refmult;
  if (smoothing_)
    raw_ref += dis_(gen_);

  if (zdc_par_.empty() || vz_par_.empty()) {
    std::cerr << "zdc and vz correction parameters must be set before "
                 "refmultcorr can be calculated"
              << std::endl;
    refmultcorr_ = refmult;
    centrality_9_ = -1;
    centrality_16_ = -1;
    weight_ = 1.0;
  }

  double zdc_scaling = zdc_par_[0] + zdc_par_[1] * zdc / 1000.0;
  double zdc_norm = zdc_par_[0] + zdc_par_[1] * zdc_norm_ / 1000.0;

  double zdc_correction = zdc_norm / zdc_scaling;

  double vz_scaling = 0.0;
  double vz_norm = 0.0;
  for (int i = 0; i < vz_par_.size(); ++i) {
    vz_scaling += vz_par_[i] * pow(vz, i);
    vz_norm += vz_par_[i] * pow(vz_norm_, i);
  }

  double vz_correction = 1.0;
  if (vz_scaling > 0.0) {
    vz_correction = vz_norm / vz_scaling;
  }

  refmultcorr_ = raw_ref * vz_correction * zdc_correction;

  // now calculate the centrality bins, both 16 & 9
  centrality_9_ = 9;
  for (int i = 0; i < cent_bin_9_.size(); ++i) {
    if (refmultcorr_ >= cent_bin_9_[cent_bin_9_.size() - i - 1]) {
      centrality_9_ = i;
      break;
    }
  }

  centrality_16_ = 16;
  for (int i = 0; i < cent_bin_16_.size(); ++i) {
    if (refmultcorr_ >= cent_bin_16_[cent_bin_16_.size() - i - 1]) {
      centrality_16_ = i;
      break;
    }
  }

  // now get the weight
  if (weight_par_.size() && centrality_9_ >= 0 && centrality_16_ >= 0 &&
      refmultcorr_ < weight_bound_) {
    double par0 = weight_par_[0];
    double par1 = weight_par_[1];
    double par2 = weight_par_[2];
    double par3 = weight_par_[3];
    double par4 = weight_par_[4];
    double par5 = weight_par_[5];
    double par6 = weight_par_[6];
    double ref_const = refmultcorr_ * par2 + par3;
    weight_ = par0 + par1 / ref_const + par4 * ref_const +
              par5 / pow(ref_const, 2.0) + par6 * pow(ref_const, 2.0);
  } else {
    weight_ = 1.0;
  }
}