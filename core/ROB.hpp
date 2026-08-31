// <ROB.hpp> -*- C++ -*-

#pragma once
#include <cstdint>
#include <cmath>
#include <Eigen/Dense>
#include <sys/types.h>

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/pevents/PeventCollector.hpp"

#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/ExpressionNodeVariables.hpp"

#include "CoreTypes.hpp"
#include "InstGroup.hpp"
#include "FlushManager.hpp"

namespace olympia
{

    /**
     * @file   ROB.h
     * @brief
     *
     * The Reorder buffer will
     * 1. retire and writeback completed instructions
     */
    class ROB : public sparta::Unit
    {
      public:
        //! \brief Parameters for ROB model
        class ROBParameterSet : public sparta::ParameterSet
        {
          public:
            ROBParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            PARAMETER(uint32_t, num_to_retire, 4, "Number of instructions to retire")
            PARAMETER(uint32_t, retire_queue_depth, 30, "Depth of the retire queue")
            PARAMETER(uint32_t, num_insts_to_retire, 0,
                      "Number of instructions to retire after which simulation will be "
                      "terminated. 0 means simulation will run until end of testcase")
            PARAMETER(bool, probabilistic_mode, false,
                      "Whether we are using the stationary distribution method to calculate IPC. "
                      "Can be set from the command line. ")
            PARAMETER(std::string, convergence_metric, "current_ipc",
                      "Options are 'current_ipc' and 'expected_ipc'")
            PARAMETER(uint64_t, convergence_check_interval, 10000,
                      "How many cycles to wait before checking if the convergence metric  "
                      "has converged when simulating in probabilistic mode")
            PARAMETER(double, convergence_threshold, 0.00005,
                      "Maximum difference between values of the convergence metric in consecutive "
                      "intervals before we "
                      "stop simulating")
            PARAMETER(uint64_t, retire_heartbeat, 1000000, "Heartbeat printout threshold")
            PARAMETER(sparta::Clock::Cycle, retire_timeout_interval, 10000,
                      "Retire timeout error threshold (in cycles). Amount of time elapsed when "
                      "nothing was retired")
        };

        /**
         * @brief Constructor for ROB
         *
         * @param node The node that represents (has a pointer to) the ROB
         * @param p The ROB's parameter set
         *
         * In the constructor for the unit, it is expected that the user
         * register the TypedPorts that this unit will need to perform
         * work.
         */
        ROB(sparta::TreeNode* node, const ROBParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

        /// Destroy!
        ~ROB();

      private:
        // Stats and counters
        sparta::StatisticDef stat_ipc_; // A simple expression to calculate IPC
        double stationary_distribution_ipc_ = 0.0;
        sparta::StatisticDef
            stat_stationary_distribution_ipc_; // IPC calculated using the stationary distribution
                                               // when simulating in probabilistic mode. We take a
                                               // reference to the stationary_distribution_ipc_
                                               // value. There might be a better way to define a
                                               // custom metric but this will do for now.
        sparta::Counter num_retired_;          // Running counter of number of instructions retired
        sparta::Counter num_uops_retired_;     // Running counter of the number of uops retired
        sparta::Counter num_flushes_;          // Number of flushes
        sparta::StatisticInstance
            overall_ipc_si_; // An overall IPC statistic instance starting at time == 0
        sparta::StatisticInstance
            period_ipc_si_; // An IPC counter for the period between retirement heartbeats

        // Parameter constants
        const sparta::Clock::Cycle retire_timeout_interval_;
        const uint32_t num_to_retire_;
        const uint32_t num_insts_to_retire_; // parameter from ilimit
        const uint64_t retire_heartbeat_;    // Retire heartbeat interval
        const bool probabilistic_mode_;
        const std::string convergence_metric_;
        uint64_t convergence_check_interval_;
        const double convergence_threshold_;
        bool terminate_without_convergence_ = false;
        InstQueue reorder_buffer_;

        Eigen::MatrixXd ipc_transition_tracker_;
        double last_recorded_ipc_; // For testing convergence when simulating in probabilistic mode;

        // For computing the stationary distribution
        uint32_t last_num_retired_ = 0;
        uint32_t prev_cycle_ipc_ = 0;

        // Bool that indicates if the ROB stopped simulation.  If
        // false and there are still instructions in the reorder
        // buffer, the machine probably has a lock up
        bool rob_stopped_simulation_{false};

        // Track a program ID to ensure the trace stream matches
        // at retirement.
        uint64_t expected_program_id_ = 1;

        // Ports used by the ROB
        sparta::DataInPort<InstGroupPtr> in_reorder_buffer_write_{&unit_port_set_,
                                                                  "in_reorder_buffer_write", 1};
        sparta::DataOutPort<uint32_t> out_reorder_buffer_credits_{&unit_port_set_,
                                                                  "out_reorder_buffer_credits"};
        sparta::DataInPort<bool> in_oldest_completed_{&unit_port_set_,
                                                      "in_reorder_oldest_completed"};
        sparta::DataOutPort<FlushManager::FlushingCriteria> out_retire_flush_{&unit_port_set_,
                                                                              "out_retire_flush"};
        // UPDATE:
        sparta::DataOutPort<InstPtr> out_rob_retire_ack_{&unit_port_set_, "out_rob_retire_ack"};
        sparta::DataOutPort<InstGroupPtr> out_rob_retire_ack_rename_{&unit_port_set_,
                                                                     "out_rob_retire_ack_rename"};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_{
            &unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        // Is the ROB expecting a flush?
        bool expect_flush_ = false;

        // Events used by the ROB
        sparta::UniqueEvent<> ev_retire_{&unit_event_set_, "retire_insts",
                                         CREATE_SPARTA_HANDLER(ROB, retireInstructions_)};

        sparta::SingleCycleUniqueEvent<> ev_track_ipc_transitions_{
            &unit_event_set_, "track_ipc_transitions",
            CREATE_SPARTA_HANDLER(ROB, trackIPCTransitions_)};

        sparta::UniqueEvent<> ev_track_current_ipc_convergence_{
            &unit_event_set_, "track_current_ipc_convergence",
            CREATE_SPARTA_HANDLER(ROB, trackCurrentIPCConvergence_)};

        // For correlation activities
        sparta::pevents::PeventCollector<InstPEventPairs> retire_event_{"RETIRE", getContainer(),
                                                                        getClock()};

        // Last inst retired for testing
        InstPtr last_inst_retired_ = nullptr;

        // A nice checker to make sure forward progress is being made
        // Note that in the ROB constructor, this event is set as non-continuing
        sparta::Clock::Cycle last_retirement_ =
            0; // Last retirement cycle for checking stalled retire
        sparta::Event<> ev_ensure_forward_progress_{
            &unit_event_set_, "forward_progress_check",
            CREATE_SPARTA_HANDLER(ROB, checkForwardProgress_)};

        std::unique_ptr<sparta::NotificationSource<bool>> rob_stopped_notif_source_;

        void sendInitialCredits_();
        void robAppended_(const InstGroup &);
        void retireInstructions_();
        void checkForwardProgress_();
        void trackIPCTransitions_();
        void trackCurrentIPCConvergence_();
        Eigen::VectorXd computeStationaryDistribution_(const Eigen::MatrixXd & matrix);
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);
        void dumpDebugContent_(std::ostream & output) const override final;
        void onStartingTeardown_() override final;

        void retireSysInst_(InstPtr &);

        // Friend class used in retire testing
        friend class ROBTester;
    };

    class ROBTester;
} // namespace olympia
