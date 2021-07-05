/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSRAN_SCHED_NR_WORKER_H
#define SRSRAN_SCHED_NR_WORKER_H

#include "sched_nr_cfg.h"
#include "sched_nr_rb_grid.h"
#include "sched_nr_ue.h"
#include "srsran/adt/circular_array.h"
#include "srsran/adt/optional.h"
#include "srsran/adt/pool/cached_alloc.h"
#include "srsran/adt/span.h"
#include <condition_variable>
#include <mutex>

namespace srsenb {
namespace sched_nr_impl {

using dl_sched_t = sched_nr_interface::dl_sched_t;
using ul_sched_t = sched_nr_interface::ul_sched_t;

class slot_cc_worker
{
public:
  explicit slot_cc_worker(const sched_cell_params& cell_params, cell_res_grid& phy_grid) :
    cfg(cell_params), res_grid(0, phy_grid)
  {}

  void start(tti_point tti_rx_, ue_map_t& ue_db_);
  void run();
  void end_tti();
  bool running() const { return tti_rx.is_valid(); }

private:
  void alloc_dl_ues();
  void alloc_ul_ues();

  const sched_cell_params& cfg;

  tti_point      tti_rx;
  slot_bwp_sched res_grid;

  srsran::static_circular_map<uint16_t, slot_ue, SCHED_NR_MAX_USERS> slot_ues;
};

class sched_worker_manager
{
  struct slot_worker_ctxt {
    std::mutex                  slot_mutex; // lock of all workers of the same slot.
    std::condition_variable     cvar;
    tti_point                   tti_rx;
    int                         nof_workers_waiting = 0;
    std::atomic<int>            worker_count{0}; // variable shared across slot_cc_workers
    std::vector<slot_cc_worker> workers;
  };

public:
  explicit sched_worker_manager(ue_map_t& ue_db_, const sched_params& cfg_);
  sched_worker_manager(const sched_worker_manager&) = delete;
  sched_worker_manager(sched_worker_manager&&)      = delete;
  ~sched_worker_manager();

  void start_slot(tti_point tti_rx, srsran::move_callback<void()> process_feedback);
  bool run_slot(tti_point tti_rx, uint32_t cc);
  void release_slot(tti_point tti_rx);
  bool get_sched_result(tti_point pdcch_tti, uint32_t cc, dl_sched_t& dl_res, ul_sched_t& ul_res);

private:
  const sched_params& cfg;
  ue_map_t&           ue_db;
  std::mutex          ue_db_mutex;

  std::vector<std::unique_ptr<slot_worker_ctxt> > slot_ctxts;

  srsran::bounded_vector<cell_res_grid, SCHED_NR_MAX_CARRIERS> cell_grid_list;

  slot_worker_ctxt& get_sf(tti_point tti_rx);
};

} // namespace sched_nr_impl
} // namespace srsenb

#endif // SRSRAN_SCHED_NR_WORKER_H
