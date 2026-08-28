// <OlympiaSim.hpp> -*- C++ -*-

#pragma once

#include "sparta/app/Simulation.hpp"
#include <cstdint>

namespace olympia
{
    class CPUFactory;
    class OlympiaAllocators;
} // namespace olympia

/*!
 * \brief OlympiaSim which builds the model and configures it
 */
class OlympiaSim : public sparta::app::Simulation
{
  public:
    /*!
     * \brief Construct OlympiaSim
     * \param scheduler The Scheduler used in simulation
     * \param num_cores Number of cores to instantiate any nodes
     *                  created which match the description as they
     *                  are created
     * \param workload The workload to run (JSON, stf trace)
     * \param instruction_limit The maximum number of instructions to
     *                          run.  0 means no limit
     * \param show_factories Print the registered factories to stdout
     * \param probabilistic_mode Whether to simulate in probabilistic mode
     * \param convergence_check_interval How many cycles to wait before checking if the transition
     * probabilities have converged when simulating in probabilistic mode
     *\param convergence_threshold Maximum difference between consecutive transition matrices before
     *we stop simulating
     */
    OlympiaSim(sparta::Scheduler & scheduler, const uint32_t num_cores, const std::string workload,
               const uint64_t instruction_limit = 0, const bool show_factories = false,
               const bool probabilistic_mode = false, const uint64_t convergence_check_interval = 0,
               const double convergence_threshold = 0.0);

    // Tear it down
    virtual ~OlympiaSim();

  private:
    //////////////////////////////////////////////////////////////////////
    // Setup

    // Allocators.  Last thing to delete
    std::unique_ptr<olympia::OlympiaAllocators> allocators_tn_;

    // The CPU TN.  This must be declared _AFTER_ the allocators so it
    // is destroyed first.
    std::unique_ptr<sparta::TreeNode> cpu_tn_to_delete_;

    //! Build the tree with tree nodes, but does not instantiate the
    //! unit yet
    void buildTree_() override;

    //! Configure the tree and apply any last minute parameter changes
    void configureTree_() override;

    //! The tree is now configured, built, and instantiated.  We need
    //! to bind things together.
    void bindTree_() override;

    //! This method is used to support command line options like
    //! --report-warmup-icount and -i
    const sparta::CounterBase* findSemanticCounter_(CounterSemantic sem) const override;

    //////////////////////////////////////////////////////////////////////
    // Runtime

    //! Number of cores in this simulator. Temporary startup option
    const uint32_t num_cores_;

    //! Workload
    const std::string workload_;

    //! Instruction limit (set up -i option on command line)
    const uint64_t instruction_limit_;
    /*!
     * \brief Get the factory for topology build
     */
    auto getCPUFactory_() -> olympia::CPUFactory*;

    /*!
     * \brief Optional flag to print registered factories to console
     */
    const bool show_factories_;

    // ! Whether to simulate in probabilistic mode
    const bool probabilistic_mode_;

    // ! How many cycles to wait before checking if the transition probabilities have converged when
    // simulating in probabilistic mode
    const uint64_t convergence_check_interval_;

    // ! Maximum difference between consecutive transition matrices before we stop simulating
    const double convergence_threshold_;
};
