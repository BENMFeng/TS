/* Copyright (C) 2015 Life Technologies Corporation, a part of Thermo Fisher Scientific, Inc. All Rights Reserved. */

//! @file     CalibrationHelper.h
//! @ingroup  Calibration
//! @brief    CalibrationHelper. Classes and methods for the Calibration executable

#ifndef CALIBRATIONHELPER_H
#define CALIBRATIONHELPER_H

#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <pthread.h>
#include "api/BamMultiReader.h"
#include "api/SamHeader.h"
#include "api/BamAlignment.h"
#include "DPTreephaser.h"
#include "BaseCallerUtils.h"
#include "json/json.h"

using namespace std;
using namespace BamTools;

class HistogramCalibration;
class LinearCalibrationModel;

// ------------------------------------------------------------------

void PrintHelp_Calibration();

// ==================================================================
// Since BamTools::BamMultiReader cannot handle BAM files aligned to multiple references
// we need our own wrapper class around multiple BAM readers
// We don't care about sorting the of the BAMs, we just need the individual alignments

class MultiBamHandler{
public:

  MultiBamHandler();

  bool    Open(vector<string> bam_names);
  void    Close();

  bool    GetNextAlignmentCore(BamAlignment & alignment);
  int     NumPasses() const { return num_bam_passes_; };
  bool    Rewind(void);
  const   BamTools::SamReadGroupDictionary &  GetReadGroups() const { return merged_read_groups_; };

private:

  // @brief
  void MergeSamHeaders();

  bool                               have_bam_files_;      //!< Did we load anything?
  bool                               no_more_data_;        //!< Did we finish reading all files?
  unsigned int                       current_bam_idx_;     //!< Index pointing to currently processed BAM
  int                                num_bam_passes_;      //!<  How often has this reader been rewound?
  vector<BamTools::BamReader *>      bam_readers_;
  vector<BamTools::SamHeader>        sam_headers_;
  BamTools::SamReadGroupDictionary   merged_read_groups_;

};


// ==================================================================
// General parameters & entities for all worker threads

struct CalibrationContext
{
  // Program threading information
  unsigned int               num_threads;                //!< Number of calibration worker threads
  unsigned int               num_reads_per_thread;       //!< Number of reads to be processed per thread
  unsigned int               num_model_reads;            //!< Accounting of how many threads read model information
  unsigned int               num_model_writes;           //!< Accounting of how many threads wrote model information
  bool                       wait_to_read_model;         //!< Signaling variable to read model information
  bool                       wait_to_write_model;        //!< Signaling variable to write model information

  pthread_mutex_t            read_mutex;                 //!< Shared data reading mutex for worker threads
  pthread_mutex_t            write_mutex;                //!< Shared data writing mutex for worker threads
  pthread_cond_t             model_read_cond;            //! Conditional variable for reading master model information
  pthread_cond_t             model_write_cond;           //! Conditional variable for writing model information to master

  // General program flow
  string                     filename_json;              //!< Path and name of calibration json output
  int                        flow_window_size;           //!< Size of a flow bin for calibration
  int                        max_num_flows;              //!< Maximum number of flows over all run ids.
  int                        verbose_level;              //!< Adjustment for the amount of feedback printout
  int                        rand_seed;                  //!< Initializes random number generator for each thread
  bool                       debug;

  bool                       successive_fit;             //!< Successively fit models in the order they are applied
  bool                       local_fit_linear_model;     //!< control training of linear model
  bool                       local_fit_polish_model;     //!< control training of polish model
  bool                       blind_fit;                  //!< treat fitting as 'blind' to reference
  int                        num_train_iterations;       //!< Blind calibration: how many loops through the data for blind calibration

  bool                       load_unmapped;              //!< Switch whether to load unmapped reads or not
  bool                       skip_droop;                 //!< Ignore droop term when generating predictions
  bool                       do_flow_alignment;          //!< Determines whether we attempt a flow alignment
  bool                       match_zero_flows;           //!< Determines if we need to exactly match incorporating flows
  float                      fill_strange_gaps;          //!< Threshold to fill in badly behaved gaps between impossible flows
  bool                       resolve_clipped_bases;      //!< Solve for hard clipped read prefix?

  // Read filtering
  unsigned int               min_mapping_qv;             //!< Minimum mapping quality for the read to be considered useful
  unsigned int               min_align_length;           //!< Minimum length of an alignment to be considered useful

  // Read Counters
  unsigned long              num_reads_in_bam;
  unsigned long              num_mapped_reads;
  unsigned long              num_loaded_reads;
  unsigned long              num_useful_reads;

  MultiBamHandler            bam_reader;                 //!< Bam reader shared by threads
  ion::ChipSubset            chip_subset;                //!< Chip coordinate & region handling for Basecaller

  vector<ion::FlowOrder >    flow_order_vector;          //!< Vector of uniquew flow orders
  map<string, int>           flow_order_index_by_run_id; //!< Map associating
  map<string, int>           num_flows_by_run_id;
  map<string, string>        key_by_read_group;          //!< Key sequence & Barcode by read group

  // *** Pointers to master calibration modules
  HistogramCalibration *     hist_calibration_master;
  LinearCalibrationModel *   linear_model_master;



  bool InitializeFromOpts(OptArgs &opts);
  void DetectFlowOrderzAndKeyFromBam(const BamTools::SamReadGroupDictionary & read_groups);
  void Verbose();
  void Close(Json::Value &json);
  void LastJsonInfo(Json::Value &json);

};


// ======================================================================

class ReadAlignmentInfo
{
public:

  ReadAlignmentInfo();

  bool UnpackReadInfo  (BamAlignment* new_alignment, vector<DPTreephaser>& treephaser_vector, const CalibrationContext& calib_context);
  bool UnpackAlignmentInfo (const CalibrationContext& calib_context);

  void GeneratePredictions (vector<DPTreephaser>& treephaser_vector,  LinearCalibrationModel& linear_model_local);

  int GetStartOfMasterRead(DPTreephaser & treephaser, BasecallerRead &master_read, const CalibrationContext& calib_context);

  void SetSize         (int flow_size);
  void Reset();


  BamAlignment *  alignment;
  vector<float>   measurements;
  int             measurements_length;     //!< Original trimmed length of the ZM measurements vector
  vector<float>   predictions_as_called;
  vector<float>   predictions_ref;
  vector<float>   state_inphase;
  vector<float>   phase_params;            //!< cf, ie, droop parameters of this read

  // Alignment independent read information
  string          run_id;                  //!< run id
  string          read_group;              //!< read group name
  string          read_bases;              //!< called bases in read direction including soft clipping
  string          prefix_bases;            //!< Hard clipped read prefix
  vector<int>     well_xy;                 //!< 2 element int vector 0-based x,y coordinate (in that order) of well on chip
  int             start_flow;              //!< Flow incorporating the first read (template) base
  int             flow_order_index;        //!< index for the flow order for this read

  // Base alignment information
  string          target_bases;            //!< Reference sequence of aligned portion of read (in read direction)
  string          query_bases;             //!< Read bases of aligned portion of read (in read direction)
  string          pretty_align;            //!< Base alignment operation string
  unsigned int    left_sc;                 //!< Number of soft clipped bases on the left side of the read (in reference direction)
  unsigned int    right_sc;                //!< Number of soft clipped bases on the right side of the read (in reference direction)
  unsigned int    start_sc;                //!< Number of soft clipped bases at the start of the read (in read direction)
  int             prefix_flow;             //!< Flow incorporating the last 5' clipped base (hard or soft)

  string          full_query_bases;         //!< Read bases including key, barcode, and 5' soft clipped bases
  string          full_target_bases;        //!< Target bases, expanded by key, barcode, and 5' clipped bases


  // Flow alignment variables
  vector<char>    aln_flow_order;           //!< The flow order for the alignment, including added / deleted flows (to make sequence fit flow order).
  vector<int>     aligned_qHPs;             //!< The HP compressed query or read sequence in the alignment, including gaps.
  vector<int>     aligned_tHPs;             //!< The HP compressed target or reference sequence in the alignment, including gaps.
  vector<int>     align_flow_index;         //!< The flow index corresponding to flow aligned query HPs, padded with -1 in gaps
  vector<char>    pretty_flow_align;        //!< The flow alignment operation string

  bool            is_filtered;

};

// ------------------------------------------------------------------

#endif // CALIBRATIONHELPER_H
