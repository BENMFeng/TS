/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#ifndef BEADTRACKER_H
#define BEADTRACKER_H

#include <set>
#include <vector>
#include "BkgMagicDefines.h"
#include "BeadParams.h"
#include "Mask.h"
#include "Region.h"
#include "SpecialDataTypes.h"
#include "GlobalDefaultsForBkgModel.h"  //to get the flow order 
#include "PinnedInFlow.h"
#include "SeqList.h"
#include "CommandLineOpts.h"
#include "BarcodeTracker.h"

class LevMarBeadAssistant;
class extern_links;
float ComputeNormalizerKeyFlows(const float *observed, const int *keyval, int len);

// possibly clashes with the 'enum' in Sequenceitem
#define LIBKEYNDX 1


// wrap a bunch of bead tracking into one place
class BeadTracker{
public:
    // bead parameters
    int  numLBeads;
    int  numLBadKey;
    bound_params params_high;
    bound_params params_low;
    std::vector<BeadParams>  params_nn;
    std::vector<SequenceItem> seqList;
    int numSeqListItems;

    int DEBUG_BEAD;
    int max_emphasis; // current maximum emphasis vector (?)
    
    // a property of the collection of beads
    // spatially oriented bead data
    // not ideally located
    // only used for xtalk
    std::vector<int> ndx_map;
    std::vector<int> key_id; // index into key map for beads

    // set up to id barcodes as an extension of key classification
    BarcodeTracker barcode_info;

    // track quality for use in regional parameter fitting
    // aggregate 'clonal', 'corrupt', 'high copy number' states
    float                   my_mean_copy_count;
    std::vector<bool>       high_quality;
    std::vector<bead_state> all_status;

    // this set is to detect when beads stop sequencing
    std::vector<float> last_amplitude; // cached variable over compute blocks
    std::vector<float> decay_ssq; // cumulative exponential tracker

    float stop_threshold; // when decay_ssq decreases below this quantity, bead vairability is so low that it is likely no longer useful
    float stop_alpha; // decay rate by flow:  discount the past
    float stop_maxvar; // just because I have a high homopolymer doesn't mean I should worry

    bool doAllBeads;
    bool ignoreQuality;
    void IgnoreQuality ();

    // track beads sampled for use in regional parameter fitting
    std::vector<bool>  sampled;
    bool isSampled;
    int ntarget;

    int regionindex; // for debugging
    
    BeadTracker();

    void  InitBeadList(Mask *bfMask, Region *region, bool ignorekey, SequenceItem *_seqList, 
        int _numSeq, const std::set<int>& sample, float AmplLowerLimit);
    void  InitBeadParams(float AmplLowerLimit);
    void  InitBeadParamR(Mask *bfMask, Region *region, std::vector<float> *tauB,std::vector<float> *tauE);
    void  InitRandomSample(Mask& bf, Region& region, const std::set<int>& sample);
    // Two use cases in the code when sampling is set
    int  SetSampled(int numSamples);
    int SetPseudoRandomSampled(int sample_level);
    int  SetSampled(std::vector<float>& penalty, int sampling_rate);
    int  SystematicSampling(int sampling_rate, std::vector<size_t>::iterator first, std::vector<size_t>::iterator last);

    // Sampled beads used for first pass region & well params
    // high_quality flag initially true for all beads
    // so all sampled beads used first pass
    // high_quality flag set to false for bad beads seen during first pass
    // and remaining sampled beads used for final region params
    bool  Sampled(int ibd) { return( sampled[ibd] && high_quality[ibd] ); }
    int   NumberSampled();

    // set our unsampled reads to good positions to start with based on sampled good beads
    void SetCopyCountOnUnSampledBeads(int flow_block_size);

    // Ignore the high_quality flag, rolling regional groups enabled
    bool  BeadIncluded (int ibd, bool skip_beads) { return ( high_quality[ibd] || !skip_beads );}

    void  InitModelBeadParams();

    void  InitLowBeadParams(float AmplLowerLimit);
    void  InitHighBeadParams();
    void  SelectDebugBead(int seed);
    void  DefineKeySequence(SequenceItem *seq_list, int number_Keys);
    // utilities for optimization
    void  AssignEmphasisForAllBeads(int max_emphasis);

    float KeyNormalizeReads(bool overwrite_key, bool sampled_only /*=false*/, int flow_block_size);
    float KeyNormalizeSampledReads(bool overwrite_key, int flow_block_size);
    float KeyNormalizeOneBead(int ibd, bool overwrite_key, int flow_block_size);
    void  SelectKeyFlowValuesFromBeadIdentity(int *keyval, float *observed, int my_key_id, int &keyLen, int flow_block_size);
    void  SelectKeyFlowValues(int *keyval,float *observed, int keyLen);
    void  ResetFlowParams(int bufNum);
    void  ResetLocalBeadParams();

    void  UpdateClonalFilter(int flow, const std::vector<float>& copy_multiplier, const PolyclonalFilterOpts & opts, int flow_block_size, int flow_begin_real_index );
    void  FinishClonalFilter( const PolyclonalFilterOpts & opts );
    int   NumHighPPF() const;
    int   NumPolyclonal() const;
    int   NumBadKey() const;
    int NumHighQuality();
    float FindMeanDmult(bool skip_beads);
    void  RescaleDmult(float scale);
    float CenterDmult(bool skip_beads);
    float FindMeanDmultFromSample();
    void  RescaleDmultFromSample(float scale);
    float CenterDmultFromSample();
    void UpdateAllPhi(int flow, int flow_block_size);
    void CenterKmult( float *mean_kmult, bool skip_beads, int *flow_ndx_map, int flow_block_size);
    bool UpdateSTOP(int ibd, int flow_block_size);

    void  RescaleRatio(float scale);
    void SetBufferingRatioOnUnSampledBeads();
    float etbRFromSampledReads();
    float etbRFromReads();
    float CopiesFromSampledReads();
     float CopiesFromReads();

    // likely not the right place for this, but does iterate over beads
    int  FindKeyID(Mask *bfMask, int ax, int ay);
    void AssignKeyID(Region *region, Mask *bfmask);
    void BuildBeadMap(Region *region, Mask *bfmask,MaskType &process_mask);
    void WriteCorruptedToMask(Region* region, Mask* bfmask, int16_t *washout_flow, int flow);
    void ZeroOutPins(Region *region, const Mask *bfmask, const PinnedInFlow &pinnedInFlow, int flow, int iFlowBuffer);
    void DumpBeads(FILE *my_fp, bool debug_only, int offset_col, int offset_row, int flow_block_size);
    void DumpAllBeadsCSV(FILE *my_fp, int flow_block_size);
    float FindPercentageCopyCount(float percentage);
    void LowCopyBeadsAreLowQuality ( float min_copy_count, float max_copy_count);
    void CorruptedBeadsAreLowQuality ();
    void TypicalBeadParams(BeadParams *p);

    void CompensateAmplitudeForEmptyWellNormalization(float *my_scale_buffer, int flow_block_size);
    //void DumpHits(int offset_col, int offset_row, int flow);
    void AssignBarcodeState(bool do_all, float basic_threshold, float tie_threshold, int flow_block_size, int flow_block_start);
    float BarcodeNormalizeOneBead(int ibd, bool overwrite_barcode, int flow_block_size);
    void CheckPointClassifier(FILE *my_fp, int id, int offset_col, int offset_row, int flow_block_size); // beads to checkpoint
private:
    void CheckKey(const std::vector<float>& copy_multiplier);
    void ComputeKeyNorm(const std::vector<int>& keyIonogram, const std::vector<float>& copy_multiplier);
    void AdjustForCopyNumber(std::vector<float>& ampl, const BeadParams& p, const std::vector<float>& copy_multiplier);
    void UpdatePPFSSQ(int flow, const std::vector<float>& copy_multiplier, const PolyclonalFilterOpts & opts, int flow_block_size, int flow_begin_real_index );

 private:
    // Boost serialization support:
    friend class boost::serialization::access;
    template<class Archive>
      void serialize(Archive& ar, const unsigned int version)
      {
	// fprintf(stdout, "Serialize BeadTracker...");
	ar & numLBeads;
	ar & numLBadKey;
	ar & params_high;
	ar & params_low;
	ar & all_status; // serialize all_status before params_nn
	ar & params_nn;  // params_nn points at bead_state objects in all_status
	ar & doAllBeads;
	ar & ignoreQuality;
	ar & seqList;
	ar & numSeqListItems;
  ar & barcode_info;
	ar & DEBUG_BEAD;
	ar & max_emphasis;
	ar & ndx_map;
	ar & key_id;
	ar & my_mean_copy_count;
	ar & high_quality;
	ar & sampled;
	ar & isSampled;
	ar & ntarget;
	ar & regionindex;
  ar & stop_alpha;
  ar & stop_threshold;
  ar & stop_maxvar;
  ar & last_amplitude;
  ar & decay_ssq;
	// fprintf(stdout, "done with BeadTracker\n");
      }
};


#endif // BEADTRACKER_H
